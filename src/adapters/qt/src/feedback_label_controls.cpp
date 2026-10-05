#include "bpr/qt/feedback_label_controls.hpp"

#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		[[nodiscard]] auto label_at(const std::size_t aIndex) noexcept -> core::feedback_label
		{
			return static_cast<core::feedback_label>(static_cast<std::uint8_t>(aIndex));
		}

		[[nodiscard]] auto state_from(const core::feedback_annotation& aAnnotation, const std::string& aBody, const bool aAwaiting) -> feedback_composition_state
		{
			auto state            = feedback_composition_state{ };
			state.Label           = aAnnotation.Label;
			state.Expectation     = aAnnotation.Expectation;
			state.AwaitingSubject = aAwaiting;
			for (const auto& decoration : aAnnotation.Decorations) {
				state.Blocking    = state.Blocking || decoration.Kind == core::feedback_decoration_kind::blocking;
				state.NonBlocking = state.NonBlocking || decoration.Kind == core::feedback_decoration_kind::non_blocking;
				state.IfMinor     = state.IfMinor || decoration.Kind == core::feedback_decoration_kind::if_minor;
			}
			state.PrefixLength = aAwaiting ? feedback_text_index(aBody, aBody.size())
										   : feedback_text_index(aBody, aAnnotation.Subject.value_or(core::feedback_span{ }).Begin);
			return state;
		}

		[[nodiscard]] auto refusal_text(const core::feedback_edit_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::feedback_edit_refusal::none:
				return { };
			case core::feedback_edit_refusal::stale_revision:
				return QStringLiteral("The comment changed; choose the label again.");
			case core::feedback_edit_refusal::invalid_edit:
				return QStringLiteral("Choose a label before a decoration.");
			case core::feedback_edit_refusal::unsupported_header:
				return QStringLiteral("The start of this comment is not a label the chip can change. Nothing was changed; edit it by hand.");
			case core::feedback_edit_refusal::restructuring_required:
				return QStringLiteral("A label cannot go before a quote, list, heading, code block, HTML or table. Nothing was changed.");
			case core::feedback_edit_refusal::body_limit:
				return QStringLiteral("With the label the comment would be too long. Nothing was changed.");
			}
			return { };
		}

		[[nodiscard]] auto label_shortcut() -> QString
		{
			return QKeySequence{ Qt::CTRL | Qt::Key_L }.toString(QKeySequence::NativeText);
		}

		[[nodiscard]] auto plain_shortcut() -> QString
		{
			return QKeySequence{ Qt::CTRL | Qt::Key_0 }.toString(QKeySequence::NativeText);
		}

		[[nodiscard]] auto is_command(const QKeyEvent& aEvent, const int aKey) -> bool
		{
			const auto modifiers = aEvent.modifiers() & ~Qt::KeypadModifier;
			return aEvent.key() == aKey && modifiers == Qt::ControlModifier;
		}

		[[nodiscard]] auto make_toggle(QWidget* const aParent, const QString& aText, const QString& aName, const QString& aTip) -> QToolButton*
		{
			auto* const button = new QToolButton{ aParent };
			button->setText(aText);
			button->setObjectName(aName);
			button->setAccessibleName(aText);
			button->setToolTip(aTip);
			button->setAccessibleDescription(aTip);
			button->setCheckable(true);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			return button;
		}
	} // namespace

	auto feedback_composition_state_of(const QString& aBody) -> feedback_composition_state
	{
		const auto body       = utf8(aBody);
		const auto annotation = core::recognize_feedback_prefix(body);
		if (annotation.Status == core::feedback_prefix_status::recognized && annotation.Subject.has_value()) {
			return state_from(annotation, body, false);
		}
		// Label first ([[BPR-1082]]): a header with no subject yet reads as
		// malformed. Probed with one subject byte, it reads as the header it
		// will be, so the chip can show and change it before the subject.
		if (annotation.Status == core::feedback_prefix_status::malformed
			&& std::ranges::find(annotation.Diagnostics, core::feedback_diagnostic::empty_subject) != annotation.Diagnostics.end()) {
			const auto probe   = body + "x";
			const auto pending = core::recognize_feedback_prefix(probe);
			if (pending.Status == core::feedback_prefix_status::recognized && pending.Subject.has_value() && pending.Subject->Begin == body.size()) {
				return state_from(pending, body, true);
			}
		}
		return { };
	}

	feedback_label_controls::feedback_label_controls(QPlainTextEdit* const aBody, QWidget* const aParent)
		: QWidget{ aParent }
		, Body{ aBody }
	{
		setObjectName(QStringLiteral("feedbackLabelControls"));
		Tints           = feedback_label_tints_of(builtin_appearance_palette(
			palette().color(QPalette::Base).lightnessF() < 0.5 ? core::appearance_variant::dark : core::appearance_variant::light));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(4);
		Chip = new QToolButton{ this };
		Chip->setObjectName(QStringLiteral("feedbackLabelChip"));
		Chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Chip->setFocusPolicy(Qt::StrongFocus);
		Chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Chip->setIconSize(QSize{ static_cast<int>(feedback_mark_size), static_cast<int>(feedback_mark_size) });
		row->addWidget(Chip);
		const auto tip = QStringLiteral("Adds (%1) to the label");
		Blocking       = make_toggle(this, QStringLiteral("blocking"), QStringLiteral("feedbackLabelBlocking"), tip.arg(QStringLiteral("blocking")));
		NonBlocking    = make_toggle(this, QStringLiteral("non-blocking"), QStringLiteral("feedbackLabelNonBlocking"), tip.arg(QStringLiteral("non-blocking")));
		IfMinor        = make_toggle(this, QStringLiteral("if-minor"), QStringLiteral("feedbackLabelIfMinor"), tip.arg(QStringLiteral("if-minor")));
		for (auto* const toggle : { Blocking, NonBlocking, IfMinor }) {
			row->addWidget(toggle);
		}
		row->addStretch();
		connect(Chip, &QToolButton::clicked, this, [this] { open_menu(); });
		connect(Blocking, &QToolButton::clicked, this, [this] { toggle_decoration(core::feedback_expectation::blocking, State.Blocking); });
		connect(NonBlocking, &QToolButton::clicked, this, [this] { toggle_decoration(core::feedback_expectation::non_blocking, State.NonBlocking); });
		connect(IfMinor, &QToolButton::clicked, this, [this] { toggle_decoration(core::feedback_expectation::if_minor, State.IfMinor); });

		Menu = new feedback_label_menu{ *this };

		Completion = new QListWidget{ this };
		Completion->setObjectName(QStringLiteral("feedbackLabelCompletion"));
		Completion->setAccessibleName(QStringLiteral("Label completions"));
		Completion->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
		Completion->setAttribute(Qt::WA_ShowWithoutActivating);
		Completion->setFocusPolicy(Qt::NoFocus);
		Completion->setIconSize(QSize{ static_cast<int>(feedback_mark_size), static_cast<int>(feedback_mark_size) });
		Completion->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Completion->hide();
		connect(Completion, &QListWidget::itemClicked, this, [this](QListWidgetItem* aItem) {
			Completion->setCurrentItem(aItem);
			take_completion();
		});

		Body->installEventFilter(this);
		connect(Body->document(), &QTextDocument::contentsChange, this, &feedback_label_controls::on_contents_change);
		connect(Body, &QPlainTextEdit::textChanged, this, &feedback_label_controls::on_text_changed);
		refresh();
	}

	feedback_label_controls::~feedback_label_controls()
	{
		delete Menu;
	}

	void feedback_label_controls::set_tints(const feedback_label_tints& aTints)
	{
		Tints = aTints;
		refresh();
	}

	void feedback_label_controls::set_helper(const bool aEnabled)
	{
		Helper = aEnabled;
		if (!Helper) {
			hide_completion();
		}
	}

	auto feedback_label_controls::helper() const noexcept -> bool
	{
		return Helper;
	}

	void feedback_label_controls::set_status_sink(std::function<void(const QString&)> aSink)
	{
		StatusSink = std::move(aSink);
	}

	auto feedback_label_controls::apply(const core::feedback_header_edit& aEdit) -> bool
	{
		if (Body->isReadOnly()) {
			return false;
		}
		const auto body       = utf8(Body->toPlainText());
		const auto annotation = core::recognize_feedback_prefix(body);
		const auto decision   = core::decide_feedback_composition_edit(core::feedback_header_edit_request{
			.Body           = body,
			.Segment        = { },
			.ExpectedDigest = annotation.BodyDigest,
			.Edit           = aEdit,
		});
		if (decision.Refusal != core::feedback_edit_refusal::none) {
			Refusal = decision.Refusal;
			++Statistics.Refusals;
			if (StatusSink) {
				StatusSink(refusal_text(decision.Refusal));
			}
			return false;
		}
		Refusal.reset();
		if (decision.Changed) {
			// One edit block is one undo step, and one change of the text is
			// one draft edit through the editor's ordinary typed path.
			auto cursor = QTextCursor{ Body->document() };
			Editing     = true;
			cursor.beginEditBlock();
			cursor.setPosition(static_cast<int>(feedback_text_index(body, decision.Replaced.Begin)));
			cursor.setPosition(static_cast<int>(feedback_text_index(body, decision.Replaced.End)), QTextCursor::KeepAnchor);
			cursor.insertText(QString::fromUtf8(decision.Replacement.data(), static_cast<qsizetype>(decision.Replacement.size())));
			cursor.endEditBlock();
			Editing = false;
			++Statistics.Edits;
		}
		refresh();
		place_caret_after_prefix();
		return true;
	}

	void feedback_label_controls::toggle_decoration(const core::feedback_expectation aExpectation, const bool aWritten)
	{
		// Setting one leaves exactly that one; pressing a written one removes
		// the standard decorations. Custom decorations are always kept.
		static_cast<void>(apply(core::feedback_header_edit{
			.Change      = core::feedback_label_change::keep,
			.Expectation = aWritten ? core::feedback_expectation::unspecified : aExpectation,
		}));
	}

	void feedback_label_controls::place_caret_after_prefix()
	{
		auto cursor = Body->textCursor();
		cursor.setPosition(static_cast<int>(std::min<qsizetype>(State.PrefixLength, Body->document()->characterCount() - 1)));
		Body->setTextCursor(cursor);
	}

	void feedback_label_controls::refresh()
	{
		State           = feedback_composition_state_of(Body->toPlainText());
		const auto open = !Body->isReadOnly();
		Chip->setEnabled(open);
		const auto shortcutTip = QStringLiteral("Label (%1), or type a code such as :qu: at the start").arg(label_shortcut());
		if (State.Label.has_value()) {
			const auto name = feedback_label_text(*State.Label);
			Chip->setText(name + QStringLiteral(" ▾"));
			Chip->setAccessibleName(QStringLiteral("Label: %1").arg(name));
			Chip->setAccessibleDescription(shortcutTip + (State.AwaitingSubject ? QStringLiteral(". The label is waiting for its subject.") : QString{ }));
			Chip->setToolTip(State.AwaitingSubject ? shortcutTip + QStringLiteral("\n%1 is waiting for its subject.").arg(name) : shortcutTip);
		} else {
			Chip->setText(QStringLiteral("Plain ▾"));
			Chip->setAccessibleName(QStringLiteral("Label: Plain comment"));
			Chip->setAccessibleDescription(shortcutTip);
			Chip->setToolTip(shortcutTip);
		}
		const auto pairs = std::array<std::pair<QToolButton*, bool>, 3U>{ { { Blocking, State.Blocking }, { NonBlocking, State.NonBlocking }, { IfMinor, State.IfMinor } } };
		for (const auto& [toggle, written] : pairs) {
			toggle->setVisible(State.Label.has_value());
			toggle->setEnabled(open && State.Label.has_value());
			toggle->setChecked(written);
		}
		refresh_icons();
		auto selections = QList<QTextEdit::ExtraSelection>{ };
		if (State.Label.has_value() && State.PrefixLength > 0) {
			// Shown while writing (owner, v4): the label is the highlighted
			// words at the start of the field.
			auto selection   = QTextEdit::ExtraSelection{ };
			selection.cursor = QTextCursor{ Body->document() };
			selection.cursor.setPosition(0);
			selection.cursor.setPosition(static_cast<int>(std::min<qsizetype>(State.PrefixLength, Body->document()->characterCount() - 1)), QTextCursor::KeepAnchor);
			const auto tint = Tints[static_cast<std::size_t>(*State.Label)];
			selection.format.setBackground(feedback_mark_soft_fill(tint, Body->palette().color(QPalette::Base)));
			selections.push_back(selection);
		}
		Body->setExtraSelections(selections);
	}

	void feedback_label_controls::refresh_icons()
	{
		const auto ratio = devicePixelRatioF();
		if (State.Label.has_value()) {
			const auto colours = feedback_mark_colours_for(Tints[static_cast<std::size_t>(*State.Label)], palette());
			Chip->setIcon(QIcon{ feedback_mark_pixmap(*State.Label, State.Expectation == core::feedback_expectation::blocking, colours, ratio) });
		} else {
			Chip->setIcon(QIcon{ feedback_plain_mark_pixmap(palette(), ratio) });
		}
	}

	void feedback_label_controls::open_menu()
	{
		if (Body->isReadOnly() || !Chip->isEnabled()) {
			return;
		}
		hide_completion();
		++Statistics.MenuOpens;
		Menu->present(State, Tints);
		Menu->adjustSize();
		auto place = Chip->mapToGlobal(QPoint{ 0, Chip->height() + 4 });
		if (const auto* const screen = Chip->screen(); screen != nullptr) {
			const auto area = screen->availableGeometry();
			place.setX(std::clamp(place.x(), area.left(), std::max(area.left(), area.right() - Menu->width())));
			if (place.y() + Menu->height() > area.bottom()) {
				place.setY(std::max(area.top(), Chip->mapToGlobal(QPoint{ 0, 0 }).y() - Menu->height() - 4));
			}
		}
		Menu->move(place);
		Menu->show();
		auto* const focus = State.Label.has_value() ? Menu->label_button(*State.Label) : Menu->plain_button();
		focus->setFocus(Qt::PopupFocusReason);
	}

	void feedback_label_controls::return_focus()
	{
		Body->setFocus(Qt::PopupFocusReason);
		place_caret_after_prefix();
	}

	auto feedback_label_controls::state() const noexcept -> const feedback_composition_state& { return State; }
	auto feedback_label_controls::chip() const noexcept -> QToolButton* { return Chip; }
	auto feedback_label_controls::blocking_toggle() const noexcept -> QToolButton* { return Blocking; }
	auto feedback_label_controls::non_blocking_toggle() const noexcept -> QToolButton* { return NonBlocking; }
	auto feedback_label_controls::if_minor_toggle() const noexcept -> QToolButton* { return IfMinor; }
	auto feedback_label_controls::menu() const noexcept -> feedback_label_menu* { return Menu; }
	auto feedback_label_controls::completion() const noexcept -> QListWidget* { return Completion; }
	auto feedback_label_controls::last_refusal() const noexcept -> std::optional<core::feedback_edit_refusal> { return Refusal; }
	auto feedback_label_controls::statistics() const noexcept -> feedback_label_control_statistics { return Statistics; }

	auto feedback_label_controls::completion_labels() const -> std::vector<core::feedback_label>
	{
		auto labels = std::vector<core::feedback_label>{ };
		for (int row = 0; row < Completion->count(); ++row) {
			labels.push_back(label_at(static_cast<std::size_t>(Completion->item(row)->data(Qt::UserRole).toInt())));
		}
		return labels;
	}

	void feedback_label_controls::on_contents_change(const int aPosition, const int aRemoved, const int aAdded)
	{
		static_cast<void>(aPosition);
		// One character typed. A paste, a deletion or a replacement is not
		// typing, so it never completes or replaces a code.
		Typed = !Editing && aRemoved == 0 && aAdded == 1;
	}

	void feedback_label_controls::on_text_changed()
	{
		if (Editing) {
			return;
		}
		refresh();
		const auto typed = std::exchange(Typed, false);
		if (!Helper || Body->isReadOnly()) {
			hide_completion();
			return;
		}
		const auto text = Body->toPlainText();
		if (!text.startsWith(QLatin1Char{ ':' })) {
			Dismissed = false;
			hide_completion();
			return;
		}
		if (!typed) {
			hide_completion();
			return;
		}
		auto letters = qsizetype{ 0 };
		while (letters < 2 && 1 + letters < text.size() && text[1 + letters] >= QLatin1Char{ 'a' } && text[1 + letters] <= QLatin1Char{ 'z' }) {
			++letters;
		}
		const auto fragment = text.mid(1, letters).toStdString();
		const auto closing  = 1 + letters < text.size() && text[1 + letters] == QLatin1Char{ ':' };
		const auto caret    = Body->textCursor().position();
		if (closing && letters == 2 && caret == 4) {
			hide_completion();
			// The closing colon of an exact code replaces the code at once. The
			// replacement waits for this change to finish, so it is its own
			// edit and its own undo step.
			if (const auto named = find_feedback_shortcode(fragment); named.has_value()) {
				const auto code = text.left(4);
				QMetaObject::invokeMethod(
					this,
					[this, code, chosen = *named] {
						if (Body->toPlainText().startsWith(code)) {
							replace_code(4, chosen);
						}
					},
					Qt::QueuedConnection);
			}
			return;
		}
		if (!closing && letters >= 1 && caret == 1 + letters && !Dismissed) {
			show_completion(feedback_shortcode_matches(fragment));
			return;
		}
		hide_completion();
	}

	void feedback_label_controls::replace_code(const qsizetype aCodeLength, const core::feedback_label aLabel)
	{
		if (Body->isReadOnly()) {
			return;
		}
		const auto text       = Body->toPlainText();
		const auto rest       = utf8(text.mid(aCodeLength));
		const auto annotation = core::recognize_feedback_prefix(rest);
		// A code in front of an existing label replaces that label; it never
		// adds a second prefix. Decorations already written are kept.
		const auto decision = core::decide_feedback_composition_edit(core::feedback_header_edit_request{
			.Body           = rest,
			.Segment        = { },
			.ExpectedDigest = annotation.BodyDigest,
			.Edit           = { .Change = core::feedback_label_change::set, .Label = aLabel },
		});
		if (decision.Refusal != core::feedback_edit_refusal::none) {
			Refusal = decision.Refusal;
			++Statistics.Refusals;
			if (StatusSink) {
				StatusSink(refusal_text(decision.Refusal));
			}
			return;
		}
		auto end         = aCodeLength;
		auto replacement = QString{ };
		if (decision.Changed) {
			replacement = QString::fromUtf8(rest.data(), static_cast<qsizetype>(decision.Replaced.Begin))
				+ QString::fromUtf8(decision.Replacement.data(), static_cast<qsizetype>(decision.Replacement.size()));
			end += feedback_text_index(rest, decision.Replaced.End);
		}
		auto cursor = QTextCursor{ Body->document() };
		Editing     = true;
		cursor.beginEditBlock();
		cursor.setPosition(0);
		cursor.setPosition(static_cast<int>(end), QTextCursor::KeepAnchor);
		cursor.insertText(replacement);
		cursor.endEditBlock();
		Editing = false;
		Refusal.reset();
		++Statistics.Edits;
		++Statistics.Shortcodes;
		Dismissed = false;
		hide_completion();
		refresh();
		place_caret_after_prefix();
	}

	void feedback_label_controls::show_completion(const std::vector<core::feedback_label>& aLabels)
	{
		if (aLabels.empty()) {
			hide_completion();
			return;
		}
		Completion->clear();
		for (const auto label : aLabels) {
			const auto code    = feedback_shortcode(label);
			auto*      item    = new QListWidgetItem{ Completion };
			const auto colours = feedback_mark_colours_for(Tints[static_cast<std::size_t>(label)], Completion->palette());
			item->setIcon(QIcon{ feedback_mark_pixmap(label, false, colours, devicePixelRatioF()) });
			item->setText(QStringLiteral(":%1:  %2").arg(QString::fromLatin1(code.data(), static_cast<qsizetype>(code.size())), feedback_label_text(label)));
			item->setToolTip(feedback_label_description(label));
			item->setData(Qt::AccessibleTextRole, feedback_label_text(label));
			item->setData(Qt::AccessibleDescriptionRole, feedback_label_description(label));
			item->setData(Qt::UserRole, static_cast<int>(label));
		}
		Completion->setCurrentRow(0);
		Completion->setToolTip(QStringLiteral("Tab or Enter replaces · : finishes a code · Esc keeps the text"));
		const auto rows   = std::min(Completion->count(), 12);
		const auto height = Completion->sizeHintForRow(0) * rows + 2 * Completion->frameWidth() + 2;
		Completion->resize(std::max(240, Completion->sizeHintForColumn(0) + 2 * Completion->frameWidth() + 8), height);
		Completion->move(Body->viewport()->mapToGlobal(Body->cursorRect().bottomLeft() + QPoint{ 0, 3 }));
		Completion->show();
		Completion->raise();
	}

	void feedback_label_controls::hide_completion()
	{
		Completion->hide();
		Completion->clear();
	}

	void feedback_label_controls::take_completion()
	{
		const auto* const item = Completion->currentItem();
		if (item == nullptr) {
			hide_completion();
			return;
		}
		const auto label = label_at(static_cast<std::size_t>(item->data(Qt::UserRole).toInt()));
		const auto text  = Body->toPlainText();
		auto       code  = qsizetype{ 1 };
		while (code < 3 && code < text.size() && text[code] >= QLatin1Char{ 'a' } && text[code] <= QLatin1Char{ 'z' }) {
			++code;
		}
		replace_code(code, label);
	}

	auto feedback_label_controls::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched != Body) {
			return QWidget::eventFilter(aWatched, aEvent);
		}
		if (aEvent->type() == QEvent::ShortcutOverride) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			// ⌘L belongs to the label menu while this body has focus, and to
			// Home's Open bar everywhere else: claiming it here means Qt never
			// sees an ambiguous shortcut (contract, derived).
			if (is_command(*key, Qt::Key_L) && !Body->isReadOnly()) {
				key->accept();
				return true;
			}
			if (Completion->isVisible() && (key->key() == Qt::Key_Escape || key->key() == Qt::Key_Tab || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
				key->accept();
				return true;
			}
		}
		if (aEvent->type() == QEvent::KeyPress) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			if (is_command(*key, Qt::Key_L) && !Body->isReadOnly()) {
				open_menu();
				return true;
			}
			if (Completion->isVisible()) {
				switch (key->key()) {
				case Qt::Key_Up:
					Completion->setCurrentRow(std::max(0, Completion->currentRow() - 1));
					return true;
				case Qt::Key_Down:
					Completion->setCurrentRow(std::min(Completion->count() - 1, Completion->currentRow() + 1));
					return true;
				case Qt::Key_Tab:
				case Qt::Key_Return:
				case Qt::Key_Enter:
					take_completion();
					return true;
				case Qt::Key_Escape:
					Dismissed = true;
					hide_completion();
					return true;
				default:
					break;
				}
			}
		}
		if (aEvent->type() == QEvent::FocusOut) {
			hide_completion();
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void feedback_label_controls::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::ApplicationPaletteChange) {
			refresh();
		}
	}

	feedback_label_menu::feedback_label_menu(feedback_label_controls& aControls)
		: QFrame{ nullptr, Qt::Popup }
		, Controls{ aControls }
	{
		setObjectName(QStringLiteral("feedbackLabelMenu"));
		setAccessibleName(QStringLiteral("Label menu"));
		setFrameShape(QFrame::StyledPanel);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(8, 8, 8, 8);
		column->setSpacing(4);
		Plain = new QToolButton{ this };
		Plain->setObjectName(QStringLiteral("feedbackLabelMenuPlain"));
		Plain->setText(QStringLiteral("Plain comment    %1").arg(plain_shortcut()));
		Plain->setAccessibleName(QStringLiteral("Plain comment"));
		Plain->setAccessibleDescription(QStringLiteral("Removes the label (%1)").arg(plain_shortcut()));
		Plain->setToolTip(QStringLiteral("No label (%1)").arg(plain_shortcut()));
		Plain->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Plain->setCheckable(true);
		Plain->setAutoRaise(true);
		Plain->setIconSize(QSize{ static_cast<int>(feedback_mark_size), static_cast<int>(feedback_mark_size) });
		column->addWidget(Plain);
		auto* const labelHeading = new QLabel{ QStringLiteral("Label"), this };
		labelHeading->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(labelHeading);
		auto* const grid = new QGridLayout{ };
		grid->setSpacing(2);
		for (std::size_t index = 0U; index < Labels.size(); ++index) {
			const auto label  = label_at(index);
			const auto code   = feedback_shortcode(label);
			auto*      button = new QToolButton{ this };
			button->setObjectName(QStringLiteral("feedbackLabelMenu_%1").arg(feedback_label_text(label)));
			button->setText(QStringLiteral("%1  :%2:").arg(feedback_label_text(label), QString::fromLatin1(code.data(), static_cast<qsizetype>(code.size()))));
			button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			button->setCheckable(true);
			button->setAutoRaise(true);
			button->setIconSize(QSize{ static_cast<int>(feedback_mark_size), static_cast<int>(feedback_mark_size) });
			button->setAccessibleName(feedback_label_text(label));
			button->setAccessibleDescription(QStringLiteral("%1 Code :%2:").arg(feedback_label_description(label), QString::fromLatin1(code.data(), static_cast<qsizetype>(code.size()))));
			button->setToolTip(feedback_label_description(label));
			button->setFocusPolicy(Qt::StrongFocus);
			grid->addWidget(button, static_cast<int>(index / 3U), static_cast<int>(index % 3U));
			Labels[index] = button;
			connect(button, &QToolButton::clicked, this, [this, label] {
				choose({ .Change = core::feedback_label_change::set, .Label = label });
			});
		}
		column->addLayout(grid);
		auto* const addsHeading = new QLabel{ QStringLiteral("Adds"), this };
		addsHeading->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(addsHeading);
		auto* const adds = new QHBoxLayout{ };
		adds->setSpacing(4);
		const auto words = std::array{ QStringLiteral("blocking"), QStringLiteral("non-blocking"), QStringLiteral("if-minor") };
		const auto names = std::array{ QStringLiteral("feedbackLabelMenuBlocking"), QStringLiteral("feedbackLabelMenuNonBlocking"), QStringLiteral("feedbackLabelMenuIfMinor") };
		const auto kinds = std::array{ core::feedback_expectation::blocking, core::feedback_expectation::non_blocking, core::feedback_expectation::if_minor };
		for (std::size_t index = 0U; index < Decorations.size(); ++index) {
			Decorations[index] = make_toggle(this, words[index], names[index], QStringLiteral("Adds (%1) to the label").arg(words[index]));
			adds->addWidget(Decorations[index]);
			const auto kind = kinds[index];
			connect(Decorations[index], &QToolButton::clicked, this, [this, index, kind] {
				const auto written = !Decorations[index]->isChecked();
				static_cast<void>(Controls.apply({ .Change = core::feedback_label_change::keep, .Expectation = written ? core::feedback_expectation::unspecified : kind }));
				present(Controls.state(), { });
			});
		}
		adds->addStretch();
		column->addLayout(adds);
		Hint = new QLabel{ this };
		Hint->setForegroundRole(QPalette::PlaceholderText);
		Hint->setText(QStringLiteral("Arrow keys move · Return chooses · Esc closes"));
		column->addWidget(Hint);
		connect(Plain, &QToolButton::clicked, this, [this] { choose({ .Change = core::feedback_label_change::remove }); });
		setTabOrder(Plain, Labels.front());
	}

	void feedback_label_menu::present(const feedback_composition_state& aState, const feedback_label_tints& aTints)
	{
		const auto ratio = devicePixelRatioF();
		const auto tints = aTints[0].isValid() ? aTints : feedback_label_tints_of(builtin_appearance_palette(palette().color(QPalette::Base).lightnessF() < 0.5 ? core::appearance_variant::dark : core::appearance_variant::light));
		Plain->setChecked(!aState.Label.has_value());
		Plain->setIcon(QIcon{ feedback_plain_mark_pixmap(palette(), ratio) });
		for (std::size_t index = 0U; index < Labels.size(); ++index) {
			const auto label = label_at(index);
			Labels[index]->setChecked(aState.Label == label);
			if (aTints[0].isValid() || Labels[index]->icon().isNull()) {
				Labels[index]->setIcon(QIcon{ feedback_mark_pixmap(label, false, feedback_mark_colours_for(tints[index], palette()), ratio) });
			}
		}
		const auto written = std::array{ aState.Blocking, aState.NonBlocking, aState.IfMinor };
		for (std::size_t index = 0U; index < Decorations.size(); ++index) {
			Decorations[index]->setEnabled(aState.Label.has_value());
			Decorations[index]->setChecked(written[index]);
		}
	}

	auto feedback_label_menu::plain_button() const noexcept -> QToolButton* { return Plain; }
	auto feedback_label_menu::label_button(const core::feedback_label aLabel) const noexcept -> QToolButton*
	{
		const auto index = static_cast<std::size_t>(aLabel);
		return index < Labels.size() ? Labels[index] : nullptr;
	}
	auto feedback_label_menu::blocking_button() const noexcept -> QToolButton* { return Decorations[0]; }
	auto feedback_label_menu::non_blocking_button() const noexcept -> QToolButton* { return Decorations[1]; }
	auto feedback_label_menu::if_minor_button() const noexcept -> QToolButton* { return Decorations[2]; }

	void feedback_label_menu::choose(const core::feedback_header_edit& aEdit)
	{
		static_cast<void>(Controls.apply(aEdit));
		close();
	}

	auto feedback_label_menu::event(QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::ShortcutOverride) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			// ⌘0 is the window's Home shortcut; inside the open menu it is
			// Plain comment, and nowhere else.
			if (is_command(*key, Qt::Key_0) || key->key() == Qt::Key_Escape || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				key->accept();
				return true;
			}
		}
		return QFrame::event(aEvent);
	}

	void feedback_label_menu::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (is_command(*aEvent, Qt::Key_0)) {
			choose({ .Change = core::feedback_label_change::remove });
			return;
		}
		switch (aEvent->key()) {
		case Qt::Key_Escape:
			close();
			return;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Space:
			if (auto* const focused = qobject_cast<QToolButton*>(focusWidget()); focused != nullptr && focused->isEnabled()) {
				focused->click();
			}
			return;
		case Qt::Key_Left:
			move_focus(0, -1);
			return;
		case Qt::Key_Right:
			move_focus(0, 1);
			return;
		case Qt::Key_Up:
			move_focus(-1, 0);
			return;
		case Qt::Key_Down:
			move_focus(1, 0);
			return;
		default:
			QFrame::keyPressEvent(aEvent);
		}
	}

	void feedback_label_menu::move_focus(const int aRows, const int aColumns)
	{
		// Row 0 is Plain, rows 1 to 4 the grid, row 5 the decorations.
		const auto at = [this](const int aRow, const int aColumn) -> QToolButton* {
			if (aRow <= 0) {
				return Plain;
			}
			if (aRow <= 4) {
				return Labels[static_cast<std::size_t>((aRow - 1) * 3 + std::clamp(aColumn, 0, 2))];
			}
			return Decorations[static_cast<std::size_t>(std::clamp(aColumn, 0, 2))];
		};
		auto row    = 0;
		auto column = 0;
		for (std::size_t index = 0U; index < Labels.size(); ++index) {
			if (focusWidget() == Labels[index]) {
				row    = static_cast<int>(index / 3U) + 1;
				column = static_cast<int>(index % 3U);
			}
		}
		for (std::size_t index = 0U; index < Decorations.size(); ++index) {
			if (focusWidget() == Decorations[index]) {
				row    = 5;
				column = static_cast<int>(index);
			}
		}
		auto* target = at(std::clamp(row + aRows, 0, 5), column + aColumns);
		if (target->isEnabled()) {
			target->setFocus(Qt::TabFocusReason);
		}
	}

	void feedback_label_menu::hideEvent(QHideEvent* const aEvent)
	{
		QFrame::hideEvent(aEvent);
		Controls.return_focus();
	}
} // namespace bpr::qt
