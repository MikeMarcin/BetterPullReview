#include "bpr/qt/private_notes_panel.hpp"

#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"

#include "conversation_glyphs.hpp"
#include "conversation_open_card.hpp"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The notes' measures, as the Comments drawer's notes have them.
		constexpr int         note_margin    = 8;
		constexpr int         note_gap       = 6;
		constexpr int         note_nudge     = -4;
		constexpr int         pad_top        = 8;
		constexpr int         pad_bottom     = 9;
		constexpr int         text_left      = 30;
		constexpr int         pad_right      = 10;
		constexpr int         chip_height    = 22;
		constexpr int         edge_band      = 34;
		constexpr int         body_lines     = 3;
		constexpr int         section_gap    = 14;
		constexpr std::size_t chips_each_way = 3U;

		[[nodiscard]] auto semibold(QFont aFont) -> QFont
		{
			aFont.setWeight(QFont::DemiBold);
			return aFont;
		}

		[[nodiscard]] auto scaled(QFont aFont, const qreal aFactor) -> QFont
		{
			if (aFont.pointSizeF() > 0.0) {
				aFont.setPointSizeF(aFont.pointSizeF() * aFactor);
			} else if (aFont.pixelSize() > 0) {
				aFont.setPixelSize(std::max(1, qRound(aFont.pixelSize() * aFactor)));
			}
			return aFont;
		}

		[[nodiscard]] auto anchor_of(const private_note_item& aNote) -> core::note_anchor
		{
			auto anchor = core::note_anchor{ };
			if (aNote.Attachment) {
				anchor.File       = aNote.Attachment->File;
				anchor.Comparison = aNote.Attachment->Comparison;
				anchor.Line       = aNote.Attachment->Line;
			}
			return anchor;
		}

		/// Up to `aLines` lines of `aText` laid out in `aWidth`, the last cut
		/// with an ellipsis when more follows.
		[[nodiscard]] auto wrapped_lines(const QString& aText, const QFont& aFont, const int aWidth, const int aLines) -> QStringList
		{
			auto       result  = QStringList{ };
			const auto metrics = QFontMetrics{ aFont };
			auto       words   = aText;
			words.replace(QLatin1Char{ '\n' }, QLatin1Char{ ' ' });
			words       = words.simplified();
			auto layout = QTextLayout{ words, aFont };
			layout.beginLayout();
			auto consumed = 0;
			while (result.size() < aLines) {
				auto line = layout.createLine();
				if (!line.isValid()) {
					break;
				}
				line.setLineWidth(std::max(1, aWidth));
				const auto piece = words.mid(line.textStart(), line.textLength());
				consumed         = line.textStart() + line.textLength();
				result.push_back(piece.trimmed());
			}
			layout.endLayout();
			if (!result.isEmpty() && consumed < words.size()) {
				result.back() = metrics.elidedText(result.back() + QStringLiteral(" ") + words.mid(consumed), Qt::ElideRight, aWidth);
			}
			return result;
		}

		/// \brief A chip at the drawer's edge: a note out of view (a dashed
		/// face and its line), or the notes elsewhere (a count).
		class note_chip final : public QAbstractButton
		{
		public:
			explicit note_chip(QWidget* aParent)
				: QAbstractButton{ aParent }
			{
				setObjectName(QStringLiteral("privateNoteEdgeChip"));
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
				setAttribute(Qt::WA_Hover, true);
				setFont(semibold(scaled(font(), 0.86)));
			}

			std::uint64_t Note  = 0U;
			bool          Face  = true;
			bool          Count = false;
			note_colours  Colours;

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { (Face ? 5 + 14 + 5 : 9) + metrics.horizontalAdvance(text()) + 8, chip_height };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				painter.setPen(QPen{ underMouse() || hasFocus() ? Colours.Faint : Colours.Rule2, 1.0 });
				painter.setBrush(Colours.Panel);
				painter.drawRoundedRect(box, height() / 2.0, height() / 2.0);
				if (hasFocus()) {
					painter.setPen(QPen{ Colours.Signal, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, height() / 2.0, height() / 2.0);
				}
				auto x = 9.0;
				if (Face) {
					// A private note's face: the lock in a dashed square.
					const auto face = QRectF{ 5.0, (height() - 14) / 2.0, 14.0, 14.0 };
					painter.setPen(QPen{ Colours.Faint, 1.2, Qt::DashLine });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(face.adjusted(1.0, 1.0, -1.0, -1.0), 3.0, 3.0);
					paint_conversation_glyph(painter, face.adjusted(3.0, 3.0, -3.0, -3.0), conversation_glyph::lock, Colours.Faint);
					x = 5.0 + 14.0 + 5.0;
				}
				painter.setPen(underMouse() ? Colours.Ink : Colours.Ink2);
				painter.drawText(QRectF{ x, 0.0, width() - x, static_cast<qreal>(height()) }, Qt::AlignLeft | Qt::AlignVCenter, text());
			}
		};

		/// \brief A quiet glyph tool in the open note: Close and Delete. Drawn
		/// in the palette on screen, so it follows the theme.
		class note_tool final : public QAbstractButton
		{
		public:
			note_tool(const conversation_glyph aGlyph, QWidget* aParent)
				: QAbstractButton{ aParent }
				, Glyph{ aGlyph }
			{
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
				setAttribute(Qt::WA_Hover, true);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override { return { 24, 24 }; }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box   = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				const auto ink   = palette().color(QPalette::Active, QPalette::WindowText);
				auto       faint = palette().color(QPalette::Active, QPalette::PlaceholderText);
				if (underMouse() && isEnabled()) {
					auto wash = ink;
					wash.setAlphaF(0.08F);
					painter.setPen(Qt::NoPen);
					painter.setBrush(wash);
					painter.drawRoundedRect(box, 5.0, 5.0);
				}
				painter.setOpacity(isEnabled() ? 1.0 : 0.4);
				paint_conversation_glyph(painter, QRectF{ (width() - 14.0) / 2.0, (height() - 14.0) / 2.0, 14.0, 14.0 }, Glyph, underMouse() ? ink : faint);
				if (hasFocus()) {
					painter.setOpacity(1.0);
					painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 1.5 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, 5.0, 5.0);
				}
			}

		private:
			conversation_glyph Glyph;
		};

		/// \brief The note's field: a bordered frame around a frameless editor,
		/// edged in the writing blue while it holds the keyboard.
		class note_field final : public QWidget
		{
		public:
			explicit note_field(QWidget* aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("privateNoteField"));
				auto* const layout = new QVBoxLayout{ this };
				layout->setContentsMargins(8, 6, 8, 6);
				Editor = new QPlainTextEdit{ this };
				Editor->setObjectName(QStringLiteral("privateNoteBody"));
				Editor->setFrameShape(QFrame::NoFrame);
				Editor->setBackgroundRole(QPalette::Base);
				Editor->viewport()->setAutoFillBackground(false);
				Editor->setTabChangesFocus(true);
				layout->addWidget(Editor);
				Editor->installEventFilter(this);
			}

			QPlainTextEdit* Editor = nullptr;
			QColor          Edge;

		protected:
			auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
			{
				if (aWatched == Editor && (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut)) {
					update();
				}
				return QWidget::eventFilter(aWatched, aEvent);
			}

			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto focused = Editor->hasFocus();
				painter.setPen(QPen{ focused ? Edge : palette().color(QPalette::Mid), focused ? 1.5 : 1.0 });
				painter.setBrush(palette().color(QPalette::Base));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.75, 0.75, -0.75, -0.75), 6.0, 6.0);
			}
		};
	} // namespace

	private_notes_panel::private_notes_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("privateNotesPanel"));
		setAccessibleName(tr("Private notes"));
		setAccessibleDescription(tr("Your private notes at their lines. Up and Down move between them, Return opens one, N writes a note on the selected line."));
		setFocusPolicy(Qt::StrongFocus);
		setMouseTracking(true);
		setAttribute(Qt::WA_Hover, true);
		setMinimumHeight(120);
		const auto action = [this](const QString& aText, const QString& aName) {
			auto* const result = new QAction{ aText, this };
			result->setObjectName(aName);
			return result;
		};
		NewLineAction = action(tr("New note on the selected line"), QStringLiteral("newLineNoteAction"));
		NewLineAction->setShortcuts(shortcut_keys(shortcut_command::new_note));
		NewLineAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		NewLineAction->setToolTip(tr("New note on the selected line (N)"));
		addAction(NewLineAction);
		NewReview = action(tr("New note on the pull request"), QStringLiteral("newReviewNoteAction"));
		NewReview->setIconText(tr("New note"));
		ElsewhereAction = action(tr("Notes on the pull request"), QStringLiteral("privateNotesElsewhereAction"));
		ElsewhereAction->setCheckable(true);
		ElsewhereAction->setToolTip(tr("Notes on the pull request, on whole files and on other files"));
		Save     = action(tr("Save"), QStringLiteral("savePrivateNoteAction"));
		Promote  = action(tr("Draft comment"), QStringLiteral("promotePrivateNoteAction"));
		AskAgent = action(tr("Ask agent about this note"), QStringLiteral("askAgentAboutPrivateNoteAction"));
		Erase    = action(tr("Delete note"), QStringLiteral("deletePrivateNoteAction"));
		Discard  = action(tr("Discard changes"), QStringLiteral("discardPrivateNoteChangesAction"));
		Close    = action(tr("Close note"), QStringLiteral("closePrivateNoteAction"));

		// The list of notes elsewhere has a header: back to the notes at
		// their lines, and a new note on the pull request.
		ListHeader = new QWidget{ this };
		ListHeader->setObjectName(QStringLiteral("privateNotesElsewhereHeader"));
		auto* const headerRow = new QHBoxLayout{ ListHeader };
		headerRow->setContentsMargins(note_margin, 6, note_margin, 6);
		headerRow->setSpacing(6);
		auto* const back = new note_chip{ ListHeader };
		back->setObjectName(QStringLiteral("privateNotesBackChip"));
		back->Face = false;
		back->setText(tr("Back to notes at their lines"));
		back->setAccessibleName(back->text());
		BackChip = back;
		headerRow->addWidget(BackChip);
		headerRow->addStretch(1);
		NewReviewButton = new QToolButton{ ListHeader };
		NewReviewButton->setObjectName(QStringLiteral("privateNotesNewReviewButton"));
		NewReviewButton->setDefaultAction(NewReview);
		NewReviewButton->setAutoRaise(true);
		NewReviewButton->setFocusPolicy(Qt::StrongFocus);
		headerRow->addWidget(NewReviewButton);
		ListHeader->hide();

		Empty = new drawer_empty_state{ drawer_icon(drawer_glyph::page, palette()), tr("Select a line and press N to write a private note on it."), NewLineAction, this };
		Empty->setObjectName(QStringLiteral("privateNotesEmpty"));
		Empty->hide();

		Notice = new QLabel{ this };
		Notice->setObjectName(QStringLiteral("privateNotesNotice"));
		Notice->setTextFormat(Qt::PlainText);
		Notice->setWordWrap(true);
		Notice->setForegroundRole(QPalette::PlaceholderText);
		Notice->setAccessibleName(tr("Private notes notice"));
		Notice->hide();

		build_open_card();

		connect(NewLineAction, &QAction::triggered, this, [this] { start_line_note(); });
		connect(NewReview, &QAction::triggered, this, [this] { start_pull_request_note(); });
		connect(ElsewhereAction, &QAction::toggled, this, [this](const bool aShown) { show_elsewhere(aShown); });
		connect(BackChip, &QAbstractButton::clicked, this, [this] { show_elsewhere(false); });
		connect(Save, &QAction::triggered, this, [this] { emit_intent(private_note_action::save); });
		connect(Erase, &QAction::triggered, this, [this] { emit_intent(private_note_action::erase); });
		connect(Promote, &QAction::triggered, this, [this] { emit_intent(private_note_action::promote); });
		connect(AskAgent, &QAction::triggered, this, [this] { emit_intent(private_note_action::ask_agent); });
		connect(Close, &QAction::triggered, this, [this] { close_note(); });
		connect(Discard, &QAction::triggered, this, [this] {
			const auto creating = Creating;
			Dirty               = false;
			Creating            = false;
			EscArmed            = false;
			NewTarget.reset();
			NewLine.reset();
			Status->clear();
			if (Dispatch) {
				Dispatch({ .Scope = Shown.Scope, .Action = private_note_action::discard });
			}
			load_editor();
			// A new note discarded is gone; a saved one shows its saved words.
			Open = Open && !creating && Editing.has_value();
			refresh_actions();
			relayout();
		});
		present({ });
	}

	void private_notes_panel::build_open_card()
	{
		const auto colours = note_colours_of(palette(), New, Route);
		auto       style   = open_card_style{ };
		style.Colours      = colours;
		Card               = new drawer_card{ this };
		Card->setObjectName(QStringLiteral("privateNoteOpenCard"));
		Card->set_state(drawer_card_state::writing);
		Card->set_ownership(drawer_ownership::private_note);
		auto* const column = new QVBoxLayout{ Card };
		column->setContentsMargins(12, 8, 10, 10);
		column->setSpacing(7);
		auto* const head = new QHBoxLayout{ };
		head->setSpacing(6);
		auto* const lock = new QLabel{ Card };
		lock->setObjectName(QStringLiteral("privateNoteLock"));
		lock->setPixmap(conversation_icon(conversation_glyph::lock, colours.Faint).pixmap(12, 12));
		head->addWidget(lock);
		CardWhere = new QLabel{ Card };
		CardWhere->setObjectName(QStringLiteral("privateNoteWhere"));
		CardWhere->setTextFormat(Qt::PlainText);
		CardWhere->setFont(semibold(font()));
		head->addWidget(CardWhere);
		CardState = new QLabel{ tr("Private note"), Card };
		CardState->setTextFormat(Qt::PlainText);
		CardState->setForegroundRole(QPalette::PlaceholderText);
		head->addWidget(CardState);
		head->addStretch(1);
		auto* const close = new note_tool{ conversation_glyph::dismiss, Card };
		close->setObjectName(QStringLiteral("privateNoteClose"));
		close->setToolTip(tr("Close (Esc)"));
		close->setAccessibleName(tr("Close note"));
		CloseButton = close;
		head->addWidget(CloseButton);
		column->addLayout(head);
		auto* const field = new note_field{ Card };
		field->Edge       = colours.Signal;
		Body              = field->Editor;
		Body->setAccessibleName(tr("Private note text"));
		Body->setAccessibleDescription(tr("Only you see this note. Save keeps it in this pull request's local ledger."));
		Body->setPlaceholderText(tr("Write a note for yourself"));
		Body->installEventFilter(this);
		field->setFixedHeight(96);
		// As tall as what is written, 96 to 260 pixels, then it scrolls.
		connect(Body->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, field, [this, field](const QSizeF aSize) {
			const auto lines  = static_cast<int>(std::ceil(aSize.height()));
			const auto height = std::clamp(lines * QFontMetrics{ Body->font() }.lineSpacing() + 16, 96, 260);
			if (field->height() != height) {
				field->setFixedHeight(height);
				relayout();
			}
		});
		column->addWidget(field);
		Status = new QLabel{ Card };
		Status->setObjectName(QStringLiteral("privateNoteStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setAccessibleName(tr("Private note status"));
		Status->setForegroundRole(QPalette::PlaceholderText);
		Status->hide();
		column->addWidget(Status);
		// [[BPR-1753]], owner look v003 ("The note we saved also has
		// some not great ux at the bottom"): the header's lock already says the
		// note is private, so the foot holds the actions alone: Delete on the
		// left, then Discard changes, Draft comment and Save, whole at any
		// width the drawer allows.
		auto* const foot = new QHBoxLayout{ };
		foot->setSpacing(6);
		auto* const remove = new note_tool{ conversation_glyph::trash, Card };
		remove->setObjectName(QStringLiteral("privateNoteDelete"));
		remove->setToolTip(tr("Delete note"));
		remove->setAccessibleName(tr("Delete note"));
		DeleteButton = remove;
		foot->addWidget(DeleteButton);
		// [[BPR-1773]]. Ask the agent about this note, with the Agent tab's
		// own mark.
		AskAgentButton = new QToolButton{ Card };
		AskAgentButton->setObjectName(QStringLiteral("privateNoteAskAgent"));
		AskAgentButton->setAutoRaise(true);
		AskAgentButton->setIcon(drawer_icon(drawer_glyph::agent, palette()));
		AskAgentButton->setIconSize(QSize{ 16, 16 });
		AskAgentButton->setAccessibleName(tr("Ask agent about this note"));
		AskAgentButton->setFocusPolicy(Qt::StrongFocus);
		foot->addWidget(AskAgentButton);
		foot->addStretch(1);
		auto* const discard = new note_button{ note_button::kind::secondary, tr("Discard changes"), Card };
		discard->setObjectName(QStringLiteral("privateNoteDiscard"));
		discard->set_style(style);
		DiscardButton = discard;
		foot->addWidget(DiscardButton);
		auto* const draft = new note_button{ note_button::kind::secondary, tr("Draft comment"), Card };
		draft->setObjectName(QStringLiteral("privateNoteDraftComment"));
		draft->set_style(style);
		DraftButton = draft;
		foot->addWidget(DraftButton);
		auto* const save = new note_button{ note_button::kind::primary, tr("Save"), Card };
		save->setObjectName(QStringLiteral("privateNoteSave"));
		save->setAccessibleName(tr("Save"));
		save->setToolTip(tr("Save (%1). Only you see this note; nothing here is published.").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
		save->set_style(style);
		SaveButton = save;
		foot->addWidget(SaveButton);
		column->addLayout(foot);
		Card->hide();

		connect(CloseButton, &QAbstractButton::clicked, Close, &QAction::trigger);
		connect(DeleteButton, &QAbstractButton::clicked, Erase, &QAction::trigger);
		connect(DiscardButton, &QAbstractButton::clicked, Discard, &QAction::trigger);
		connect(DraftButton, &QAbstractButton::clicked, Promote, &QAction::trigger);
		connect(AskAgentButton, &QAbstractButton::clicked, AskAgent, &QAction::trigger);
		connect(SaveButton, &QAbstractButton::clicked, Save, &QAction::trigger);
		connect(Body, &QPlainTextEdit::textChanged, this, [this] {
			if (!Applying) {
				Dirty    = Creating || (Editing && Body->toPlainText().toStdString() != Editing->Text);
				EscArmed = false;
				refresh_actions();
				relayout();
			}
		});
	}

	void private_notes_panel::set_dispatch(private_note_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void private_notes_panel::set_line_locator(private_note_line_locator aLocator) { Locator = std::move(aLocator); }
	void private_notes_panel::set_comparison_source(std::function<const core::file_alignment*()> aSource) { Comparison = std::move(aSource); }
	void private_notes_panel::set_selected_line_source(std::function<std::optional<private_note_line>()> aSource) { SelectedLine = std::move(aSource); }
	void private_notes_panel::set_line_dispatch(std::function<void(core::diff_side, std::uint32_t)> aDispatch) { LineDispatch = std::move(aDispatch); }
	void private_notes_panel::set_elsewhere_dispatch(std::function<void(const private_note_item&)> aDispatch) { ElsewhereDispatch = std::move(aDispatch); }
	void private_notes_panel::set_marks_observer(std::function<void()> aObserver) { MarksObserver = std::move(aObserver); }
	void private_notes_panel::set_scroll_forward(std::function<bool(QWheelEvent&)> aForward) { ScrollForward = std::move(aForward); }

	void private_notes_panel::set_colours(const QColor& aNew, const QColor& aRoute)
	{
		if (New == aNew && Route == aRoute) {
			return;
		}
		New          = aNew;
		Route        = aRoute;
		auto colours = drawer_card_colours{ };
		if (aRoute.isValid()) {
			colours.Reading = aRoute;
		}
		colours.Writing = palette().color(QPalette::Active, QPalette::Highlight);
		Card->set_colours(colours);
		refresh_open_card();
		update();
	}

	void private_notes_panel::present(private_notes_projection aProjection)
	{
		const auto changedScope = Shown.Scope != aProjection.Scope;
		Shown                   = std::move(aProjection);
		if (changedScope) {
			Selected = 0U;
			Current  = 0U;
			Editing.reset();
			Creating = false;
			Open     = false;
			Dirty    = false;
			EscArmed = false;
			NewTarget.reset();
			NewLine.reset();
			Status->clear();
		}
		if (!Dirty && !Creating) {
			load_editor();
		}
		if (!Shown.Available && Open) {
			Status->setText(tr("Private notes are unavailable for this review. Your unsaved text is kept."));
		}
		refresh_actions();
		relayout();
	}

	auto private_notes_panel::item(const std::uint64_t aId) const -> const private_note_item*
	{
		const auto found = std::ranges::find(Shown.Notes, aId, &private_note_item::Id);
		return found == Shown.Notes.end() ? nullptr : &*found;
	}

	auto private_notes_panel::laid(const std::uint64_t aId) const -> const laid_note*
	{
		const auto found = std::ranges::find(Laid, aId, &laid_note::Id);
		return found == Laid.end() ? nullptr : &*found;
	}

	auto private_notes_panel::placement(const std::uint64_t aId) const -> core::note_placement
	{
		const auto* const note = item(aId);
		if (note == nullptr) {
			return { };
		}
		return core::place_note(anchor_of(*note), Comparison ? Comparison() : nullptr);
	}

	void private_notes_panel::begin_note(std::optional<private_note_target> aTarget, std::optional<private_note_line> aLine)
	{
		if (!Shown.Available) {
			return;
		}
		if (Dirty) {
			refuse(tr("Save or discard your changes before starting another note."));
			return;
		}
		Notice->clear();
		Selected = 0U;
		Editing.reset();
		Creating  = true;
		Open      = true;
		EscArmed  = false;
		NewTarget = std::move(aTarget);
		NewLine   = aLine;
		Applying  = true;
		Body->clear();
		Applying = false;
		Dirty    = false;
		Status->clear();
		refresh_actions();
		relayout();
		Body->setFocus(Qt::OtherFocusReason);
	}

	void private_notes_panel::start_line_note()
	{
		const auto* const shown = Comparison ? Comparison() : nullptr;
		const auto        line  = SelectedLine ? SelectedLine() : std::nullopt;
		if (!Shown.Available || shown == nullptr || !Shown.CurrentFile || Shown.CurrentFile->Comparison != shown->Digest || !line) {
			refuse(tr("Select a line in the code to write a note on it."));
			return;
		}
		const auto anchor = core::note_line_anchor_at(*shown, line->Side, line->Line);
		if (!anchor) {
			refuse(tr("That line is not in this comparison."));
			return;
		}
		auto target = *Shown.CurrentFile;
		target.Line = anchor;
		show_elsewhere(false);
		begin_note(target, line);
	}

	void private_notes_panel::start_pull_request_note()
	{
		// Written at the head of the notes on the pull request.
		if (!Dirty) {
			show_elsewhere(true);
		}
		begin_note(std::nullopt, std::nullopt);
	}

	void private_notes_panel::open_note(const std::uint64_t aId)
	{
		const auto* const note = item(aId);
		if (note == nullptr) {
			return;
		}
		if (Dirty && aId != Selected) {
			refuse(tr("Save or discard your changes before opening another note."));
			return;
		}
		Notice->clear();
		Selected = aId;
		Current  = aId;
		Creating = false;
		Open     = true;
		EscArmed = false;
		NewTarget.reset();
		NewLine.reset();
		Status->clear();
		load_editor();
		const auto place = placement(aId);
		if (place.Kind == core::note_placement_kind::other_file) {
			// Its file first, then its line; it opens there.
			show_elsewhere(false);
			if (ElsewhereDispatch) {
				ElsewhereDispatch(*note);
			}
		} else if (place.Kind == core::note_placement_kind::pull_request || place.Kind == core::note_placement_kind::whole_file) {
			show_elsewhere(true);
		} else if (place.Kind == core::note_placement_kind::placed && place.At) {
			show_elsewhere(false);
			const auto y = Locator ? Locator(place.At->Side, place.At->Line) : std::nullopt;
			if ((!y || *y < 0 || *y > height()) && LineDispatch) {
				LineDispatch(place.At->Side, place.At->Line);
			}
		}
		refresh_actions();
		relayout();
		Body->setFocus(Qt::OtherFocusReason);
		Body->moveCursor(QTextCursor::End);
	}

	void private_notes_panel::close_note()
	{
		if (!Open) {
			return;
		}
		if (Dirty) {
			if (!EscArmed) {
				EscArmed = true;
				Status->setText(tr("Not saved. Save with %1, or press Esc again to discard your changes.").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
				refresh_actions();
				relayout();
				return;
			}
			Discard->trigger();
		}
		Open     = false;
		Creating = false;
		EscArmed = false;
		NewTarget.reset();
		NewLine.reset();
		Status->clear();
		refresh_actions();
		relayout();
		setFocus(Qt::OtherFocusReason);
	}

	void private_notes_panel::show_elsewhere(const bool aShown)
	{
		if (Elsewhere != aShown) {
			Elsewhere  = aShown;
			ListScroll = 0;
		}
		const auto blocker = QSignalBlocker{ ElsewhereAction };
		ElsewhereAction->setChecked(aShown);
		relayout();
	}

	auto private_notes_panel::elsewhere_shown() const noexcept -> bool { return Elsewhere; }
	auto private_notes_panel::open_note_id() const noexcept -> std::uint64_t { return Open && !Creating ? Selected : 0U; }
	auto private_notes_panel::writing() const noexcept -> bool { return Open; }
	auto private_notes_panel::notes() const noexcept -> const std::vector<private_note_item>& { return Shown.Notes; }

	void private_notes_panel::load_editor()
	{
		const auto* const found = item(Selected);
		Editing                 = found == nullptr ? std::nullopt : std::optional{ *found };
		if (!Editing) {
			Selected = 0U;
		}
		Applying = true;
		Body->setPlainText(Editing ? QString::fromStdString(Editing->Text) : QString{ });
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
		if (!Shown.Available || !Editing || Dirty || QString::fromStdString(Editing->Text).trimmed().isEmpty() || Editing->Text.size() > core::maximum_shared_draft_body_bytes) {
			return false;
		}
		// A note at its line drafts there; one without a line drafts at
		// Comment's current target, as before.
		const auto place = placement(Editing->Id);
		if (Editing->Attachment && Editing->Attachment->Line) {
			return place.Kind == core::note_placement_kind::placed;
		}
		return TargetAvailable;
	}

	void private_notes_panel::refresh_actions()
	{
		const auto* const shown = Comparison ? Comparison() : nullptr;
		const auto        line  = SelectedLine ? SelectedLine() : std::nullopt;
		// Offered with a file shown; pressed without a selected line, it says
		// how to choose one.
		NewLineAction->setEnabled(Shown.Available && shown != nullptr);
		NewLineAction->setStatusTip(line.has_value() ? tr("Write a private note on the selected line") : tr("Select a line in the code to write a note on it."));
		NewReview->setEnabled(Shown.Available);
		ElsewhereAction->setEnabled(Shown.Available);
		Body->setReadOnly(!(Shown.Available && (Creating || Editing)));
		const auto body = Body->toPlainText();
		Save->setEnabled(Shown.Available && Dirty && !body.trimmed().isEmpty());
		Save->setToolTip(tr("Save privately in this pull request's local ledger."));
		Erase->setEnabled(Shown.Available && Editing.has_value() && !Dirty);
		Erase->setToolTip(Dirty ? tr("Save or discard your changes before deleting this note.") : tr("Delete this private note."));
		Discard->setEnabled(Dirty || Creating);
		Promote->setEnabled(can_promote());
		const auto lined = Editing && Editing->Attachment && Editing->Attachment->Line;
		Promote->setToolTip(!Editing                                                           ? tr("Save the note first; then it can become a draft.")
								: Dirty                                                        ? tr("Save your note before drafting a comment from it.")
								: Editing->Text.size() > core::maximum_shared_draft_body_bytes ? tr("This note is too long for a comment.")
								: lined && !Promote->isEnabled()                               ? tr("Its line is not in this comparison, so it has nowhere to draft a comment.")
								: !lined && !TargetAvailable                                   ? TargetReason
								: lined                                                        ? tr("Copy this note to a local draft at its line. The note stays; nothing is published.")
																							   : tr("Copy this note to a local draft at Comment's target. The note stays; nothing is published."));
		SaveButton->setEnabled(Save->isEnabled());
		DeleteButton->setVisible(Editing.has_value() && !Creating);
		DeleteButton->setEnabled(Erase->isEnabled());
		DiscardButton->setVisible(Dirty && !Creating && Editing.has_value());
		DraftButton->setVisible(!Creating);
		DraftButton->setEnabled(Promote->isEnabled());
		DraftButton->setToolTip(Promote->toolTip());
		AskAgent->setEnabled(Shown.Available && Editing.has_value() && !Dirty && !Creating && !Editing->Text.empty());
		AskAgent->setToolTip(Dirty ? tr("Save your note before asking the agent about it.")
								   : tr("Open the Agent tab with this note attached to your next question. The note is saved in Private history as it is now and sent as that copy."));
		AskAgentButton->setVisible(Editing.has_value() && !Creating);
		AskAgentButton->setEnabled(AskAgent->isEnabled());
		AskAgentButton->setToolTip(AskAgent->toolTip());
		Status->setVisible(!Status->text().isEmpty());
	}

	void private_notes_panel::emit_intent(const private_note_action aAction)
	{
		refresh_actions();
		const auto allowed = aAction == private_note_action::save ? Save->isEnabled() : aAction == private_note_action::erase ? Erase->isEnabled()
			: aAction == private_note_action::ask_agent                                                                       ? AskAgent->isEnabled()
																															  : can_promote();
		if (!Dispatch || !allowed) {
			return;
		}
		Dispatch({ .Scope = Shown.Scope, .Action = aAction, .Id = Editing ? Editing->Id : 0U, .Revision = Editing ? Editing->Revision : 0U, .Attachment = Editing ? Editing->Attachment : NewTarget, .Text = Body->toPlainText().toStdString() });
	}

	void private_notes_panel::acknowledge(const std::uint64_t aSavedId)
	{
		// Saved or deleted: the note closes and hangs at its line again.
		Selected = aSavedId;
		Current  = aSavedId;
		Dirty    = false;
		Creating = false;
		Open     = false;
		EscArmed = false;
		NewTarget.reset();
		NewLine.reset();
		Status->clear();
		load_editor();
		refresh_actions();
		relayout();
	}

	void private_notes_panel::refuse(const QString& aReason)
	{
		// Said in the open note, or at the panel's foot when none is open.
		if (Open) {
			Status->setText(aReason);
		} else {
			Notice->setText(aReason);
		}
		refresh_actions();
		relayout();
	}

	auto private_notes_panel::where_of(const private_note_item& aNote, const core::note_placement& aPlace) const -> QString
	{
		switch (aPlace.Kind) {
		case core::note_placement_kind::pull_request: return tr("Pull request");
		case core::note_placement_kind::whole_file:   return tr("Whole file");
		case core::note_placement_kind::placed:       return tr("Line %1").arg(aPlace.At ? aPlace.At->Line + 1U : 0U);
		case core::note_placement_kind::outdated:     return tr("Was line %1").arg(aNote.Attachment && aNote.Attachment->Line ? aNote.Attachment->Line->Line + 1U : 0U);
		case core::note_placement_kind::other_file:   break;
		}
		auto label = QString::fromStdString(aNote.Label);
		if (aNote.Attachment && aNote.Attachment->Line) {
			return tr("%1, line %2").arg(label).arg(aNote.Attachment->Line->Line + 1U);
		}
		return label;
	}

	auto private_notes_panel::sentence_of(const core::note_placement& aPlace) const -> QString
	{
		if (aPlace.Kind != core::note_placement_kind::outdated) {
			return { };
		}
		switch (aPlace.Reason) {
		case core::note_outdated_reason::content_changed: return tr("The file changed since you wrote this, so its line is not shown.");
		case core::note_outdated_reason::side_absent:     return tr("That side of the file is gone in this comparison.");
		case core::note_outdated_reason::unmatched:
		case core::note_outdated_reason::line_absent:
		case core::note_outdated_reason::none:            break;
		}
		return tr("Its line is not in this comparison.");
	}

	auto private_notes_panel::closed_height(const private_note_item& aNote, const QString& aWhere, const QString& aSentence, const int aWidth) const -> int
	{
		const auto metrics = QFontMetrics{ font() };
		const auto small   = QFontMetrics{ scaled(font(), 0.92) };
		const auto lines   = wrapped_lines(QString::fromStdString(aNote.Text), font(), aWidth - text_left - pad_right, body_lines);
		const auto where   = !aWhere.isEmpty();
		auto       height  = pad_top + (where ? small.height() + 4 : 0) + static_cast<int>(lines.size()) * metrics.lineSpacing() + pad_bottom;
		if (!aSentence.isEmpty()) {
			height += static_cast<int>(wrapped_lines(aSentence, scaled(font(), 0.92), aWidth - text_left - pad_right, 2).size()) * small.lineSpacing() + 2;
		}
		return height;
	}

	auto private_notes_panel::order() const -> std::vector<std::uint64_t>
	{
		auto result = std::vector<std::uint64_t>{ };
		for (const auto& note : Laid) {
			if (note.Shown && note.Id != 0U) {
				result.push_back(note.Id);
			}
		}
		return result;
	}

	void private_notes_panel::relayout()
	{
		const auto* const shown                    = Comparison ? Comparison() : nullptr;
		const auto        colours                  = note_colours_of(palette(), New, Route);
		const auto        width                    = this->width() - 2 * note_margin;
		static_cast<note_chip*>(BackChip)->Colours = colours;
		Laid.clear();
		auto counts   = std::array<std::size_t, 3U>{ }; // pull request, whole file, other files
		auto placed   = std::vector<laid_note>{ };
		auto outdated = std::vector<laid_note>{ };
		auto others   = std::vector<laid_note>{ };
		for (const auto& note : Shown.Notes) {
			auto entry  = laid_note{ };
			entry.Id    = note.Id;
			entry.Place = core::place_note(anchor_of(note), shown);
			entry.Where = where_of(note, entry.Place);
			switch (entry.Place.Kind) {
			case core::note_placement_kind::placed:
				entry.TargetY = Locator && entry.Place.At ? Locator(entry.Place.At->Side, entry.Place.At->Line).value_or(-100000) : -100000;
				placed.push_back(std::move(entry));
				break;
			case core::note_placement_kind::outdated:
				entry.Sentence = sentence_of(entry.Place);
				outdated.push_back(std::move(entry));
				break;
			case core::note_placement_kind::pull_request:
				++counts[0];
				others.push_back(std::move(entry));
				break;
			case core::note_placement_kind::whole_file:
				++counts[1];
				others.push_back(std::move(entry));
				break;
			case core::note_placement_kind::other_file:
				++counts[2];
				others.push_back(std::move(entry));
				break;
			}
		}
		std::ranges::stable_sort(placed, { }, &laid_note::TargetY);
		auto       openTop   = std::optional<int>{ };
		auto       openNotch = std::optional<int>{ };
		auto       above     = std::vector<std::uint64_t>{ };
		auto       below     = std::vector<std::uint64_t>{ };
		const auto openId    = Open && !Creating ? Selected : 0U;
		const auto cardFor   = [&](const int aWidth) {
			Card->setFixedWidth(std::max(160, aWidth));
			Card->layout()->activate();
			const auto h = Card->layout()->hasHeightForWidth() ? Card->layout()->totalHeightForWidth(Card->width()) : Card->sizeHint().height();
			return std::max(h, Card->minimumSizeHint().height());
		};
		if (!Elsewhere) {
			// A new note on a line takes its place among them, at its line.
			if (Open && Creating && NewLine) {
				auto entry    = laid_note{ };
				entry.Id      = 0U;
				entry.Place   = core::note_placement{ .Kind = core::note_placement_kind::placed, .Reason = core::note_outdated_reason::none, .At = shown ? std::optional{ core::source_line_ref{ .Comparison = shown->Digest, .Side = NewLine->Side, .Line = NewLine->Line } } : std::nullopt };
				entry.Where   = tr("Line %1").arg(NewLine->Line + 1U);
				entry.TargetY = Locator ? Locator(NewLine->Side, NewLine->Line).value_or(edge_band) : edge_band;
				placed.push_back(std::move(entry));
				std::ranges::stable_sort(placed, { }, &laid_note::TargetY);
			}
			// Above is a line above the code in view; a line under the top
			// chips is still in view, and its note sits just below them.
			const auto anyAbove   = std::ranges::any_of(placed, [](const laid_note& aNote) { return aNote.TargetY + 10 < 0; });
			const auto topBand    = counts[0] > 0U || anyAbove ? edge_band : 0;
			const auto bottomEdge = height() - (counts[1] + counts[2] > 0U ? edge_band : 0);
			auto       cursor     = topBand + 2;
			for (auto& entry : placed) {
				const auto        isOpen = (entry.Id == 0U && Creating) || (entry.Id != 0U && entry.Id == openId);
				const auto* const note   = item(entry.Id);
				const auto        height = isOpen ? cardFor(width) : (note != nullptr ? closed_height(*note, entry.Where, entry.Sentence, width) : 0);
				if (!isOpen && entry.TargetY + 10 < 0) {
					above.push_back(entry.Id);
					Laid.push_back(std::move(entry));
					continue;
				}
				auto top = std::max(entry.TargetY + note_nudge, cursor);
				if (isOpen) {
					// The open note stays in view, its notch at its line when the
					// line is.
					top       = std::clamp(top, topBand + 2, std::max(topBand + 2, bottomEdge - height - 2));
					openTop   = top;
					openNotch = entry.TargetY >= top && entry.TargetY <= top + height ? std::optional{ entry.TargetY - top + 9 } : std::nullopt;
				} else if (top + height > bottomEdge - (below.empty() ? 0 : edge_band)) {
					below.push_back(entry.Id);
					Laid.push_back(std::move(entry));
					continue;
				}
				entry.Rect  = QRect{ note_margin, top, width, height };
				entry.Shown = true;
				cursor      = top + height + note_gap;
				Laid.push_back(std::move(entry));
			}
			// Outdated notes follow the placed ones, each saying why.
			for (auto& entry : outdated) {
				const auto        isOpen = entry.Id == openId;
				const auto* const note   = item(entry.Id);
				const auto        height = isOpen ? cardFor(width) : (note != nullptr ? closed_height(*note, entry.Where, entry.Sentence, width) : 0);
				if (!isOpen && cursor + height > bottomEdge) {
					below.push_back(entry.Id);
					Laid.push_back(std::move(entry));
					continue;
				}
				if (isOpen) {
					openTop = cursor;
					openNotch.reset();
				}
				entry.Rect  = QRect{ note_margin, cursor, width, height };
				entry.Shown = true;
				cursor += height + note_gap;
				Laid.push_back(std::move(entry));
			}
			for (auto& entry : others) {
				Laid.push_back(std::move(entry));
			}
		} else {
			// Elsewhere: the pull request's notes, then the whole file's, then
			// other files', each a quiet section, in a list that scrolls.
			const auto headerHeight = ListHeader->sizeHint().height();
			auto       cursor       = headerHeight + 4 - ListScroll;
			if (Open && Creating && !NewLine) {
				openTop = cursor;
				cursor += cardFor(width) + section_gap;
			}
			const auto small = QFontMetrics{ scaled(font(), 0.92) };
			for (const auto kind : { core::note_placement_kind::pull_request, core::note_placement_kind::whole_file, core::note_placement_kind::other_file, core::note_placement_kind::outdated }) {
				auto first = true;
				for (auto& entry : kind == core::note_placement_kind::outdated ? outdated : others) {
					if (entry.Place.Kind != kind || entry.Id == 0U) {
						continue;
					}
					// The section says where; the note need not say it again.
					if (kind == core::note_placement_kind::pull_request || kind == core::note_placement_kind::whole_file) {
						entry.Where.clear();
					}
					if (first) {
						cursor += small.height() + 6;
						first = false;
					}
					const auto        isOpen = entry.Id == openId;
					const auto* const note   = item(entry.Id);
					const auto        height = isOpen ? cardFor(width) : (note != nullptr ? closed_height(*note, entry.Where, entry.Sentence, width) : 0);
					if (isOpen) {
						openTop = cursor;
					}
					entry.Rect  = QRect{ note_margin, cursor, width, height };
					entry.Shown = true;
					cursor += height + note_gap;
				}
				if (!first) {
					cursor += section_gap - note_gap;
				}
			}
			for (auto& list : { std::ref(others), std::ref(outdated), std::ref(placed) }) {
				for (auto& entry : list.get()) {
					Laid.push_back(std::move(entry));
				}
			}
		}

		// The open note, in place.
		if (openTop && Open) {
			Card->setGeometry(note_margin, *openTop, std::max(160, width), cardFor(width));
			Card->set_notch(openNotch);
			refresh_open_card();
			Card->show();
			Card->raise();
		} else {
			Card->hide();
		}

		// The header of the notes elsewhere.
		ListHeader->setVisible(Elsewhere);
		if (Elsewhere) {
			ListHeader->setGeometry(0, 0, this->width(), ListHeader->sizeHint().height());
			ListHeader->raise();
		}

		// The chips at the edges.
		auto       used     = std::size_t{ 0U };
		const auto nextChip = [&]() -> note_chip* {
			if (used == Chips.size()) {
				auto* const chip = new note_chip{ this };
				connect(chip, &QAbstractButton::clicked, this, [this, chip] {
					const auto* const own = static_cast<note_chip*>(chip);
					if (own->Count) {
						show_elsewhere(true);
						return;
					}
					open_note(own->Note);
				});
				Chips.push_back(chip);
			}
			auto* const chip = static_cast<note_chip*>(Chips[used++]);
			chip->Colours    = colours;
			return chip;
		};
		if (!Elsewhere) {
			auto       x     = note_margin;
			const auto right = this->width() - note_margin;
			const auto place = [&](note_chip* aChip, const int aY) {
				aChip->updateGeometry();
				const auto size = aChip->sizeHint();
				if (x + size.width() > right && x > note_margin) {
					aChip->hide();
					--used;
					return false;
				}
				aChip->setGeometry(x, aY, std::min(size.width(), right - x), chip_height);
				aChip->show();
				aChip->raise();
				aChip->update();
				x += size.width() + 4;
				return true;
			};
			const auto    topY      = (edge_band - chip_height) / 2;
			const auto    bottomY   = height() - edge_band + (edge_band - chip_height) / 2;
			const QString counted[] = { tr("%n on the pull request", nullptr, static_cast<int>(counts[0])), tr("%n on the whole file", nullptr, static_cast<int>(counts[1])),
										tr("%n on other files", nullptr, static_cast<int>(counts[2])) };
			const auto    countChip = [&](const std::size_t aIndex, const int aY) {
				if (counts[aIndex] == 0U) {
					return true;
				}
				auto* const chip = nextChip();
				chip->Count      = true;
				chip->Face       = aIndex == 0U;
				chip->Note       = 0U;
				chip->setText(counted[aIndex]);
				chip->setToolTip(tr("Show the notes on the pull request, on whole files and on other files"));
				chip->setAccessibleName(counted[aIndex]);
				return place(chip, aY);
			};
			// The pull request's notes lead the top edge, as the mockup has
			// them; the whole file's and other files' close the bottom edge.
			static_cast<void>(countChip(0U, topY));
			const auto lineChips = [&](const std::vector<std::uint64_t>& aNotes, const int aY, const bool aDown) {
				for (std::size_t at = 0U; at < std::min(aNotes.size(), chips_each_way); ++at) {
					const auto* const entry = laid(aNotes[at]);
					if (entry == nullptr || !entry->Place.At) {
						continue;
					}
					auto* const chip = nextChip();
					chip->Count      = false;
					chip->Face       = true;
					chip->Note       = entry->Id;
					chip->setText(QString::number(entry->Place.At->Line + 1U));
					const auto* const note = item(entry->Id);
					chip->setToolTip(note != nullptr ? tr("Line %1: %2").arg(entry->Place.At->Line + 1U).arg(QString::fromStdString(note->Text).left(70)) : QString{ });
					chip->setAccessibleName(tr("Private note on line %1, %2").arg(entry->Place.At->Line + 1U).arg(aDown ? tr("below") : tr("above")));
					if (!place(chip, aY)) {
						break;
					}
				}
			};
			lineChips(above, topY, false);
			x = note_margin;
			lineChips(below, bottomY, true);
			if (countChip(1U, bottomY)) {
				static_cast<void>(countChip(2U, bottomY));
			}
		}
		for (auto index = used; index < Chips.size(); ++index) {
			Chips[index]->hide();
		}

		// Nothing to show: say how to write one.
		const auto anything = std::ranges::any_of(Laid, [](const laid_note& aNote) { return aNote.Shown; }) || used > 0U || Card->isVisible();
		Empty->setVisible(!anything && !Elsewhere);
		if (Empty->isVisible()) {
			auto* const sentence = Empty->findChild<QLabel*>(QStringLiteral("drawerEmptySentence"));
			const auto  words    = !Shown.Available ? tr("Private notes are unavailable for this review.")
				: shown == nullptr                  ? tr("Open a file to write private notes at its lines. Notes on the pull request are under the pull request tool.")
													: tr("Select a line and press N to write a private note on it.");
			if (sentence != nullptr && sentence->text() != words) {
				sentence->setText(words);
			}
			Empty->setGeometry(rect());
			Empty->lower();
		}

		// Why the last gesture did nothing, at the foot, while no note is open.
		Notice->setVisible(!Open && !Notice->text().isEmpty());
		if (Notice->isVisible()) {
			const auto h = Notice->heightForWidth(this->width() - 2 * note_margin - 8);
			Notice->setGeometry(note_margin + 4, height() - h - 6 - (below.empty() ? 0 : edge_band), this->width() - 2 * note_margin - 8, h);
			Notice->raise();
		}
		ListBottom = 0;
		for (const auto& entry : Laid) {
			if (entry.Shown) {
				ListBottom = std::max(ListBottom, entry.Rect.bottom());
			}
		}
		if (Card->isVisible()) {
			ListBottom = std::max(ListBottom, Card->geometry().bottom());
		}

		// The gutter: a dashed cell on each noted line, washed blue while open.
		auto left  = std::vector<private_note_mark>{ };
		auto right = std::vector<private_note_mark>{ };
		for (const auto& entry : Laid) {
			if (entry.Place.Kind != core::note_placement_kind::placed || !entry.Place.At) {
				continue;
			}
			auto mark        = private_note_mark{ };
			mark.Line        = entry.Place.At->Line;
			mark.Open        = Open && ((entry.Id == 0U && Creating) || (entry.Id != 0U && entry.Id == openId));
			mark.Description = tr("Line %1: your private note").arg(entry.Place.At->Line + 1U);
			(entry.Place.At->Side == core::diff_side::left ? left : right).push_back(std::move(mark));
		}
		set_marks(std::move(left), std::move(right));
		update();
	}

	void private_notes_panel::refresh_open_card()
	{
		if (Card == nullptr) {
			return;
		}
		// The buttons and the lock in the theme on screen.
		const auto colours = note_colours_of(palette(), New, Route);
		auto       style   = open_card_style{ };
		style.Colours      = colours;
		for (auto* const button : { DiscardButton, DraftButton, SaveButton }) {
			static_cast<note_button*>(button)->set_style(style);
		}
		for (const auto* name : { "privateNoteLock" }) {
			if (auto* const lock = Card->findChild<QLabel*>(QString::fromLatin1(name)); lock != nullptr) {
				lock->setPixmap(conversation_icon(conversation_glyph::lock, colours.Faint).pixmap(12, 12));
			}
		}
		if (auto* const field = Card->findChild<QWidget*>(QStringLiteral("privateNoteField")); field != nullptr) {
			static_cast<note_field*>(field)->Edge = colours.Signal;
		}

		auto where = QString{ };
		if (Creating) {
			where = NewLine ? tr("Line %1").arg(NewLine->Line + 1U) : NewTarget ? QString::fromStdString(Shown.CurrentFileLabel)
																				: tr("Pull request");
		} else if (const auto* const entry = laid(Selected); entry != nullptr) {
			where = entry->Where.isEmpty() ? where_of(*item(Selected), entry->Place) : entry->Where;
		}
		if (CardWhere->text() != where) {
			CardWhere->setText(where);
		}
		CardState->setText(Creating ? tr("New private note") : tr("Private note"));
	}

	void private_notes_panel::set_marks(std::vector<private_note_mark> aLeft, std::vector<private_note_mark> aRight)
	{
		if (aLeft == LeftMarks && aRight == RightMarks) {
			return;
		}
		LeftMarks  = std::move(aLeft);
		RightMarks = std::move(aRight);
		if (MarksObserver) {
			MarksObserver();
		}
	}

	auto private_notes_panel::line_marks(const core::diff_side aSide) const -> std::vector<private_note_mark>
	{
		return aSide == core::diff_side::left ? LeftMarks : RightMarks;
	}

	auto private_notes_panel::note_on_line(const core::diff_side aSide, const std::uint32_t aLine) const -> std::optional<std::uint64_t>
	{
		for (const auto& entry : Laid) {
			if (entry.Id != 0U && entry.Place.Kind == core::note_placement_kind::placed && entry.Place.At && entry.Place.At->Side == aSide && entry.Place.At->Line == aLine) {
				return entry.Id;
			}
		}
		return std::nullopt;
	}

	void private_notes_panel::refresh_geometry()
	{
		refresh_actions();
		relayout();
	}

	auto private_notes_panel::note_rect(const std::uint64_t aId) const -> QRect
	{
		if (Open && aId == Selected && !Creating && Card->isVisible()) {
			return Card->geometry();
		}
		const auto* const entry = laid(aId);
		return entry != nullptr && entry->Shown ? entry->Rect : QRect{ };
	}

	auto private_notes_panel::edge_chips() const -> std::vector<QAbstractButton*>
	{
		auto result = std::vector<QAbstractButton*>{ };
		for (auto* const chip : Chips) {
			if (chip->isVisible()) {
				result.push_back(chip);
			}
		}
		return result;
	}

	auto private_notes_panel::hit(const QPoint aPoint) const -> std::optional<std::uint64_t>
	{
		for (const auto& entry : Laid) {
			if (entry.Shown && entry.Id != 0U && entry.Rect.contains(aPoint) && !(Open && entry.Id == Selected && !Creating)) {
				return entry.Id;
			}
		}
		return std::nullopt;
	}

	void private_notes_panel::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto colours     = note_colours_of(palette(), New, Route);
		auto       cardColours = drawer_card_colours{ };
		if (Route.isValid()) {
			cardColours.Reading = Route;
		}
		cardColours.Writing = colours.Signal;
		const auto small    = scaled(font(), 0.92);
		const auto smallM   = QFontMetrics{ small };
		const auto metrics  = QFontMetrics{ font() };
		// Section headings of the notes elsewhere.
		if (Elsewhere) {
			auto lastKind = std::optional<core::note_placement_kind>{ };
			for (const auto& entry : Laid) {
				if (!entry.Shown || entry.Place.Kind == lastKind) {
					continue;
				}
				lastKind         = entry.Place.Kind;
				const auto title = entry.Place.Kind == core::note_placement_kind::pull_request ? tr("Pull request")
					: entry.Place.Kind == core::note_placement_kind::whole_file                ? tr("Whole file")
					: entry.Place.Kind == core::note_placement_kind::outdated                  ? tr("Not at a line in this comparison")
																							   : tr("Other files");
				painter.setFont(semibold(small));
				painter.setPen(colours.Faint);
				painter.drawText(QRect{ note_margin + 4, entry.Rect.top() - smallM.height() - 4, width() - 2 * note_margin, smallM.height() }, Qt::AlignLeft | Qt::AlignVCenter, title);
			}
		}
		for (const auto& entry : Laid) {
			if (!entry.Shown || entry.Id == 0U || (Open && entry.Id == Selected && !Creating)) {
				continue;
			}
			const auto* const note = item(entry.Id);
			if (note == nullptr) {
				continue;
			}
			const auto rect    = entry.Rect;
			const auto hovered = entry.Id == Hovered;
			const auto focused = hasFocus() && entry.Id == Current;
			paint_drawer_card(painter, QRectF{ rect }.adjusted(0.5, 0.5, -0.5, -0.5), palette(), drawer_card_state::closed, drawer_ownership::private_note, cardColours, hovered || focused);
			if (focused) {
				painter.setPen(QPen{ colours.Signal, 2.0 });
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(QRectF{ rect }.adjusted(1.0, 1.0, -1.0, -1.0), 8.0, 8.0);
			}
			// The lock, where a thread's face is.
			paint_conversation_glyph(painter, QRectF{ static_cast<qreal>(rect.left() + 11), static_cast<qreal>(rect.top() + pad_top + 1), 12.0, 12.0 }, conversation_glyph::lock, colours.Faint);
			auto y = rect.top() + pad_top;
			if (entry.Place.Kind == core::note_placement_kind::placed && entry.Place.At) {
				// Its line, boxed in the code's figures, as the mockup draws it.
				const auto number = QString::number(entry.Place.At->Line + 1U);
				const auto box    = QRectF{ static_cast<qreal>(rect.left() + text_left), static_cast<qreal>(y), static_cast<qreal>(smallM.horizontalAdvance(number) + 10), static_cast<qreal>(smallM.height()) };
				painter.setPen(QPen{ colours.Rule2, 1.0 });
				painter.setBrush(colours.Hover);
				painter.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
				painter.setFont(semibold(small));
				painter.setPen(colours.Ink2);
				painter.drawText(box, Qt::AlignCenter, number);
				y += smallM.height() + 4;
			} else if (!entry.Where.isEmpty()) {
				painter.setFont(semibold(small));
				painter.setPen(colours.Ink2);
				painter.drawText(QRect{ rect.left() + text_left, y, rect.width() - text_left - pad_right, smallM.height() }, Qt::AlignLeft | Qt::AlignVCenter,
								 smallM.elidedText(entry.Where, Qt::ElideMiddle, rect.width() - text_left - pad_right));
				y += smallM.height() + 4;
			}
			if (!entry.Sentence.isEmpty()) {
				painter.setFont(small);
				painter.setPen(colours.Faint);
				for (const auto& line : wrapped_lines(entry.Sentence, small, rect.width() - text_left - pad_right, 2)) {
					painter.drawText(QRect{ rect.left() + text_left, y, rect.width() - text_left - pad_right, smallM.lineSpacing() }, Qt::AlignLeft | Qt::AlignVCenter, line);
					y += smallM.lineSpacing();
				}
				y += 2;
			}
			painter.setFont(font());
			painter.setPen(colours.Ink);
			for (const auto& line : wrapped_lines(QString::fromStdString(note->Text), font(), rect.width() - text_left - pad_right, body_lines)) {
				painter.drawText(QRect{ rect.left() + text_left, y, rect.width() - text_left - pad_right, metrics.lineSpacing() }, Qt::AlignLeft | Qt::AlignVCenter, line);
				y += metrics.lineSpacing();
			}
		}
	}

	void private_notes_panel::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		relayout();
	}

	void private_notes_panel::mousePressEvent(QMouseEvent* aEvent)
	{
		if (aEvent->button() == Qt::LeftButton) {
			if (const auto id = hit(aEvent->position().toPoint()); id.has_value()) {
				open_note(*id);
				aEvent->accept();
				return;
			}
		}
		setFocus(Qt::MouseFocusReason);
		QWidget::mousePressEvent(aEvent);
	}

	void private_notes_panel::mouseMoveEvent(QMouseEvent* aEvent)
	{
		const auto id = hit(aEvent->position().toPoint()).value_or(0U);
		if (id != Hovered) {
			Hovered = id;
			setCursor(id != 0U ? Qt::PointingHandCursor : Qt::ArrowCursor);
			update();
		}
		QWidget::mouseMoveEvent(aEvent);
	}

	void private_notes_panel::leaveEvent(QEvent* aEvent)
	{
		if (Hovered != 0U) {
			Hovered = 0U;
			update();
		}
		QWidget::leaveEvent(aEvent);
	}

	void private_notes_panel::keyPressEvent(QKeyEvent* aEvent)
	{
		const auto ids = order();
		const auto at  = std::ranges::find(ids, Current);
		switch (aEvent->key()) {
		case Qt::Key_Down:
		case Qt::Key_Up:   {
			if (ids.empty()) {
				break;
			}
			auto index = at == ids.end() ? (aEvent->key() == Qt::Key_Down ? 0 : static_cast<std::ptrdiff_t>(ids.size()) - 1)
										 : std::clamp<std::ptrdiff_t>((at - ids.begin()) + (aEvent->key() == Qt::Key_Down ? 1 : -1), 0, static_cast<std::ptrdiff_t>(ids.size()) - 1);
			Current    = ids[static_cast<std::size_t>(index)];
			update();
			aEvent->accept();
			return;
		}
		case Qt::Key_Return:
		case Qt::Key_Enter:
			if (Current != 0U) {
				open_note(Current);
				aEvent->accept();
				return;
			}
			break;
		case Qt::Key_Escape:
			if (Elsewhere && !Open) {
				show_elsewhere(false);
				aEvent->accept();
				return;
			}
			if (Open) {
				close_note();
				aEvent->accept();
				return;
			}
			break;
		default:
			break;
		}
		QWidget::keyPressEvent(aEvent);
	}

	void private_notes_panel::wheelEvent(QWheelEvent* aEvent)
	{
		if (!Elsewhere) {
			// The notes hang at the code's lines: the wheel moves the code,
			// and they follow it.
			if (ScrollForward && ScrollForward(*aEvent)) {
				aEvent->accept();
				return;
			}
			aEvent->ignore();
			return;
		}
		ListScroll = std::clamp(ListScroll - aEvent->angleDelta().y() / 4, 0, std::max(0, ListBottom + ListScroll - height() + 12));
		relayout();
		aEvent->accept();
	}

	void private_notes_panel::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			auto colours = drawer_card_colours{ };
			if (Route.isValid()) {
				colours.Reading = Route;
			}
			colours.Writing = palette().color(QPalette::Active, QPalette::Highlight);
			Card->set_colours(colours);
			relayout();
		}
	}

	auto private_notes_panel::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aWatched == Body && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && key->modifiers().testFlag(Qt::ControlModifier)) {
				Save->trigger();
				return true;
			}
			if (key->key() == Qt::Key_Escape) {
				close_note();
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	auto private_notes_panel::open_card() const noexcept -> QWidget* { return Card->isVisible() ? Card : nullptr; }
	auto private_notes_panel::empty_state() const noexcept -> drawer_empty_state* { return Empty; }
	auto private_notes_panel::status_label() const noexcept -> QLabel* { return Status; }
	auto private_notes_panel::notice_label() const noexcept -> QLabel* { return Notice; }
	auto private_notes_panel::body_editor() const noexcept -> QPlainTextEdit* { return Body; }
	auto private_notes_panel::new_line_action() const noexcept -> QAction* { return NewLineAction; }
	auto private_notes_panel::new_review_action() const noexcept -> QAction* { return NewReview; }
	auto private_notes_panel::elsewhere_action() const noexcept -> QAction* { return ElsewhereAction; }
	auto private_notes_panel::save_action() const noexcept -> QAction* { return Save; }
	auto private_notes_panel::erase_action() const noexcept -> QAction* { return Erase; }
	auto private_notes_panel::promote_action() const noexcept -> QAction* { return Promote; }
	auto private_notes_panel::discard_action() const noexcept -> QAction* { return Discard; }
	auto private_notes_panel::close_action() const noexcept -> QAction* { return Close; }
}
