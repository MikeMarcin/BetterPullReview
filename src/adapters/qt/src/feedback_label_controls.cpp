#include "bpr/qt/feedback_label_controls.hpp"

#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QSizePolicy>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
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
			state.AwaitingSubject = aAwaiting;
			for (const auto& decoration : aAnnotation.Decorations) {
				state.Blocking    = state.Blocking || decoration.Kind == core::feedback_decoration_kind::blocking;
				state.NonBlocking = state.NonBlocking || decoration.Kind == core::feedback_decoration_kind::non_blocking;
				state.IfMinor     = state.IfMinor || decoration.Kind == core::feedback_decoration_kind::if_minor;
				if (decoration.Kind == core::feedback_decoration_kind::custom) {
					const auto raw = core::feedback_text(aBody, decoration.Raw);
					const auto own = QString::fromUtf8(raw.data(), static_cast<qsizetype>(raw.size()));
					if (!state.Custom.contains(own, Qt::CaseInsensitive)) {
						state.Custom.push_back(own);
					}
				}
			}
			state.Emphasis     = feedback_mark_emphasis_of(state.Blocking, state.NonBlocking, state.IfMinor);
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

		// [[BPR-1346]]. The macOS style draws a tool button's icon flush
		// against its text (owner, BPR-1091 v002: "Can we add like a tiny
		// space between the icon and the word?"). The mark's icon carries the
		// gap as transparent pixels at its right, so every style keeps it and
		// it scales with the mark.
		constexpr auto mark_word_gap = 4;

		[[nodiscard]] auto mark_icon_size() -> QSize
		{
			return QSize{ static_cast<int>(feedback_mark_size) + mark_word_gap, static_cast<int>(feedback_mark_size) };
		}

		[[nodiscard]] auto mark_icon(const QPixmap& aMark) -> QIcon
		{
			const auto ratio  = aMark.devicePixelRatio();
			const auto shape  = mark_icon_size();
			auto       padded = QPixmap{ static_cast<int>(std::ceil(shape.width() * ratio)), static_cast<int>(std::ceil(shape.height() * ratio)) };
			padded.setDevicePixelRatio(ratio);
			padded.fill(Qt::transparent);
			auto painter = QPainter{ &padded };
			painter.drawPixmap(QPointF{ 0.0, 0.0 }, aMark);
			painter.end();
			return QIcon{ padded };
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

	auto readable_link_colour(const QPalette& aPalette) -> QColor
	{
		auto       link   = aPalette.color(QPalette::Active, QPalette::Link);
		const auto ground = aPalette.color(QPalette::Active, QPalette::Window);
		if (ground.lightness() >= 128 || link.lightness() >= 165) {
			return link;
		}
		// The Markdown reader's lightness first, then lighter until the link
		// reads at 4.5:1 on the ground: a palette's default blue is about
		// 4.2:1 there at 175.
		const auto hue        = link.hslHue() < 0 ? 210 : link.hslHue();
		const auto saturation = std::min(link.hslSaturation(), 200);
		for (auto lightness = 175; lightness <= 235; lightness += 5) {
			link = QColor::fromHsl(hue, saturation, lightness);
			if (contrast_ratio(to_appearance_color(link), to_appearance_color(ground)) >= 4.5) {
				break;
			}
		}
		return link;
	}

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
		Theme           = default_feedback_mark_theme(palette());
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(4);
		Chip = new QToolButton{ this };
		Chip->setObjectName(QStringLiteral("feedbackLabelChip"));
		Chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Chip->setFocusPolicy(Qt::StrongFocus);
		Chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Chip->setIconSize(mark_icon_size());
		row->addWidget(Chip);
		anchor_tooltips(*Chip);
		const auto tip = QStringLiteral("Adds (%1) to the label");
		Blocking       = make_toggle(this, QStringLiteral("blocking"), QStringLiteral("feedbackLabelBlocking"), tip.arg(QStringLiteral("blocking")));
		NonBlocking    = make_toggle(this, QStringLiteral("non-blocking"), QStringLiteral("feedbackLabelNonBlocking"), tip.arg(QStringLiteral("non-blocking")));
		IfMinor        = make_toggle(this, QStringLiteral("if-minor"), QStringLiteral("feedbackLabelIfMinor"), tip.arg(QStringLiteral("if-minor")));
		for (auto* const toggle : { Blocking, NonBlocking, IfMinor }) {
			row->addWidget(toggle);
			anchor_tooltips(*toggle);
		}
		row->addStretch();
		connect(Chip, &QToolButton::clicked, this, [this] { open_menu(); });
		connect(Blocking, &QToolButton::clicked, this, [this] { toggle_decoration(QStringLiteral("blocking"), State.Blocking); });
		connect(NonBlocking, &QToolButton::clicked, this, [this] { toggle_decoration(QStringLiteral("non-blocking"), State.NonBlocking); });
		connect(IfMinor, &QToolButton::clicked, this, [this] { toggle_decoration(QStringLiteral("if-minor"), State.IfMinor); });

		Menu = new feedback_label_menu{ *this };

		Body->installEventFilter(this);
		connect(Body, &QPlainTextEdit::textChanged, this, &feedback_label_controls::on_text_changed);
		refresh();
	}

	feedback_label_controls::~feedback_label_controls()
	{
		delete Menu;
	}

	void feedback_label_controls::set_theme(const feedback_mark_theme& aTheme)
	{
		Theme = aTheme;
		refresh();
	}

	void feedback_label_controls::set_decoration_toggles(const bool aShown)
	{
		Toggles = aShown;
		refresh();
	}

	void feedback_label_controls::set_status_sink(std::function<void(const QString&)> aSink)
	{
		StatusSink = std::move(aSink);
	}

	void feedback_label_controls::set_reference_dispatch(std::function<void(core::reference_page)> aDispatch)
	{
		ReferenceDispatch = std::move(aDispatch);
	}

	auto feedback_label_controls::reference_offered() const noexcept -> bool
	{
		return static_cast<bool>(ReferenceDispatch);
	}

	void feedback_label_controls::open_reference(const core::reference_page aPage)
	{
		if (!ReferenceDispatch) {
			return;
		}
		++Statistics.References;
		// The window's answer can rebuild the editor that holds these
		// controls; keep the callback alive for its own call.
		const auto dispatch = ReferenceDispatch;
		dispatch(aPage);
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

	void feedback_label_controls::toggle_decoration(const QString& aToken, const bool aWritten)
	{
		// One decoration in or out; every other one stays as written
		// ([[BPR-1083]] v004). Blocking and non-blocking take each other out.
		static_cast<void>(apply(core::feedback_header_edit{
			.Change     = core::feedback_label_change::keep,
			.Decoration = core::feedback_decoration_change{ .Token = utf8(aToken), .Written = !aWritten },
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
		const auto shortcutTip = QStringLiteral("Label (%1)").arg(label_shortcut());
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
			toggle->setVisible(Toggles && State.Label.has_value());
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
			const auto tint = Theme.Tints[static_cast<std::size_t>(*State.Label)];
			selection.format.setBackground(feedback_mark_soft_fill(tint, Body->palette().color(QPalette::Base)));
			selections.push_back(selection);
		}
		Body->setExtraSelections(selections);
	}

	void feedback_label_controls::refresh_icons()
	{
		const auto ratio = devicePixelRatioF();
		if (State.Label.has_value()) {
			const auto colours = feedback_mark_colours_for(Theme, *State.Label, palette());
			Chip->setIcon(mark_icon(feedback_mark_pixmap(*State.Label, State.Emphasis, colours, ratio)));
		} else {
			Chip->setIcon(mark_icon(feedback_plain_mark_pixmap(palette(), ratio)));
		}
	}

	void feedback_label_controls::open_menu()
	{
		if (Body->isReadOnly() || !Chip->isEnabled()) {
			return;
		}
		++Statistics.MenuOpens;
		Menu->present(State, Theme);
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

	auto feedback_label_controls::theme() const noexcept -> const feedback_mark_theme& { return Theme; }
	auto feedback_label_controls::state() const noexcept -> const feedback_composition_state& { return State; }
	auto feedback_label_controls::chip() const noexcept -> QToolButton* { return Chip; }
	auto feedback_label_controls::blocking_toggle() const noexcept -> QToolButton* { return Blocking; }
	auto feedback_label_controls::non_blocking_toggle() const noexcept -> QToolButton* { return NonBlocking; }
	auto feedback_label_controls::if_minor_toggle() const noexcept -> QToolButton* { return IfMinor; }
	auto feedback_label_controls::menu() const noexcept -> feedback_label_menu* { return Menu; }
	auto feedback_label_controls::last_refusal() const noexcept -> std::optional<core::feedback_edit_refusal> { return Refusal; }
	auto feedback_label_controls::statistics() const noexcept -> feedback_label_control_statistics { return Statistics; }

	void feedback_label_controls::on_text_changed()
	{
		if (Editing) {
			return;
		}
		refresh();
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
		}
		if (aEvent->type() == QEvent::KeyPress) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			if (is_command(*key, Qt::Key_L) && !Body->isReadOnly()) {
				open_menu();
				return true;
			}
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
		Plain->setIconSize(mark_icon_size());
		anchor_tooltips(*Plain);
		column->addWidget(Plain);
		auto* const labelHeading = new QLabel{ QStringLiteral("Label"), this };
		labelHeading->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(labelHeading);
		auto* const grid = new QGridLayout{ };
		grid->setSpacing(2);
		for (std::size_t index = 0U; index < Labels.size(); ++index) {
			const auto  label  = label_at(index);
			auto* const button = new QToolButton{ this };
			button->setObjectName(QStringLiteral("feedbackLabelMenu_%1").arg(feedback_label_text(label)));
			button->setText(feedback_label_text(label));
			button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			button->setCheckable(true);
			button->setAutoRaise(true);
			button->setIconSize(mark_icon_size());
			button->setAccessibleName(feedback_label_text(label));
			button->setAccessibleDescription(feedback_label_description(label));
			button->setToolTip(feedback_label_description(label));
			button->setFocusPolicy(Qt::StrongFocus);
			anchor_tooltips(*button);
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
		for (std::size_t index = 0U; index < Decorations.size(); ++index) {
			Decorations[index] = make_toggle(this, words[index], names[index], QStringLiteral("Adds (%1) to the label").arg(words[index]));
			adds->addWidget(Decorations[index]);
			anchor_tooltips(*Decorations[index]);
			connect(Decorations[index], &QToolButton::clicked, this, [this, index, word = words[index]] {
				// The click has already flipped the check: checked means write it.
				decorate(word, Decorations[index]->isChecked());
			});
		}
		adds->addStretch();
		column->addLayout(adds);
		// [[BPR-1083]] v004. Custom decorations: those written show as
		// toggles here, and the field writes a new one as typed.
		CustomRow           = new QWidget{ this };
		auto* const customs = new QHBoxLayout{ CustomRow };
		customs->setContentsMargins(0, 0, 0, 0);
		customs->setSpacing(4);
		customs->addStretch();
		column->addWidget(CustomRow);
		auto* const own = new QHBoxLayout{ };
		own->setSpacing(4);
		CustomField = new QLineEdit{ this };
		CustomField->setObjectName(QStringLiteral("feedbackLabelMenuCustomField"));
		CustomField->setPlaceholderText(QStringLiteral("Your own, such as security"));
		CustomField->setAccessibleName(QStringLiteral("Custom decoration"));
		CustomField->setAccessibleDescription(QStringLiteral("One word of letters, digits, - or _, added to the label as written. Return adds it."));
		CustomField->setValidator(new QRegularExpressionValidator{ QRegularExpression{ QStringLiteral("[A-Za-z0-9_-]{1,%1}").arg(core::maximum_feedback_label_bytes) }, CustomField });
		CustomField->setMaxLength(static_cast<int>(core::maximum_feedback_label_bytes));
		own->addWidget(CustomField, 1);
		CustomAdd = new QToolButton{ this };
		CustomAdd->setObjectName(QStringLiteral("feedbackLabelMenuCustomAdd"));
		CustomAdd->setText(QStringLiteral("Add"));
		CustomAdd->setAccessibleName(QStringLiteral("Add custom decoration"));
		CustomAdd->setAutoRaise(true);
		CustomAdd->setFocusPolicy(Qt::StrongFocus);
		own->addWidget(CustomAdd);
		column->addLayout(own);
		connect(CustomField, &QLineEdit::returnPressed, this, [this] { add_custom(); });
		connect(CustomAdd, &QToolButton::clicked, this, [this] { add_custom(); });
		connect(CustomField, &QLineEdit::textChanged, this, [this](const QString& aText) { CustomAdd->setEnabled(CustomField->isEnabled() && !aText.isEmpty()); });
		Hint = new QLabel{ this };
		Hint->setForegroundRole(QPalette::PlaceholderText);
		Hint->setText(QStringLiteral("Arrow keys move · Return chooses · Esc closes"));
		column->addWidget(Hint);
		// [[BPR-1566]]. The owner, 2026-10-16: a small way to the page the
		// labels come from, "for people who don't know where those come from
		// or how to use them". Under a rule and in the link colour, so it
		// reads as a way out rather than a thirteenth label. It names the
		// page; the window hands the name to the application, which builds
		// the address and opens it. Shown only when a window bound it.
		AboutRule = new QFrame{ this };
		AboutRule->setObjectName(QStringLiteral("feedbackLabelMenuAboutRule"));
		AboutRule->setFrameShape(QFrame::HLine);
		AboutRule->setFrameShadow(QFrame::Plain);
		AboutRule->setForegroundRole(QPalette::Midlight);
		column->addWidget(AboutRule);
		About = new QToolButton{ this };
		About->setObjectName(QStringLiteral("feedbackLabelMenuAbout"));
		About->setText(QStringLiteral("About Conventional Comments"));
		About->setAccessibleName(QStringLiteral("About Conventional Comments"));
		About->setAccessibleDescription(QStringLiteral("Where these labels come from and how to use them. Opens conventionalcomments.org in your browser."));
		About->setToolTip(QStringLiteral("Where these labels come from and how to use them.\nOpens conventionalcomments.org in your browser."));
		About->setAutoRaise(true);
		About->setFocusPolicy(Qt::StrongFocus);
		About->setCursor(Qt::PointingHandCursor);
		anchor_tooltips(*About);
		column->addWidget(About, 0, Qt::AlignLeft);
		connect(About, &QToolButton::clicked, this, [this] {
			// Not a choice: the text is untouched. The menu closes first, so
			// focus is back in the comment before the browser takes it.
			close();
			Controls.open_reference(core::reference_page::conventional_comments);
		});
		connect(Plain, &QToolButton::clicked, this, [this] { choose({ .Change = core::feedback_label_change::remove }); });
		setTabOrder(Plain, Labels.front());
	}

	void feedback_label_menu::present(const feedback_composition_state& aState, const feedback_mark_theme& aTheme)
	{
		const auto ratio = devicePixelRatioF();
		Plain->setChecked(!aState.Label.has_value());
		Plain->setIcon(mark_icon(feedback_plain_mark_pixmap(palette(), ratio)));
		for (std::size_t index = 0U; index < Labels.size(); ++index) {
			const auto label = label_at(index);
			Labels[index]->setChecked(aState.Label == label);
			Labels[index]->setIcon(mark_icon(feedback_mark_pixmap(label, feedback_mark_emphasis{ }, feedback_mark_colours_for(aTheme, label, palette()), ratio)));
		}
		const auto written = std::array{ aState.Blocking, aState.NonBlocking, aState.IfMinor };
		for (std::size_t index = 0U; index < Decorations.size(); ++index) {
			Decorations[index]->setEnabled(aState.Label.has_value());
			Decorations[index]->setChecked(written[index]);
		}
		// The custom decorations written, one checked toggle each; unchecking
		// one takes it out.
		const auto hadFocus = std::ranges::any_of(Custom, [](const QToolButton* aButton) { return aButton->hasFocus(); });
		for (auto* const button : Custom) {
			button->deleteLater();
			button->hide();
		}
		Custom.clear();
		auto* const row = static_cast<QHBoxLayout*>(CustomRow->layout());
		for (const auto& token : aState.Custom) {
			auto* const button = make_toggle(CustomRow, token, QStringLiteral("feedbackLabelMenuCustom_%1").arg(token), QStringLiteral("Adds (%1) to the label").arg(token));
			button->setChecked(true);
			anchor_tooltips(*button);
			row->insertWidget(row->count() - 1, button);
			connect(button, &QToolButton::clicked, this, [this, token, button] { decorate(token, button->isChecked()); });
			Custom.push_back(button);
		}
		CustomRow->setVisible(!Custom.empty());
		CustomField->setEnabled(aState.Label.has_value());
		CustomAdd->setEnabled(aState.Label.has_value() && !CustomField->text().isEmpty());
		const auto offered = Controls.reference_offered();
		AboutRule->setVisible(offered);
		About->setVisible(offered);
		{
			// The link colour of the palette on screen, read again at every
			// opening so a change of appearance is followed.
			auto linkPalette = About->palette();
			linkPalette.setColor(QPalette::ButtonText, readable_link_colour(palette()));
			About->setPalette(linkPalette);
		}
		if (hadFocus) {
			(Custom.empty() ? static_cast<QWidget*>(CustomField) : Custom.front())->setFocus(Qt::TabFocusReason);
		}
		adjustSize();
	}

	void feedback_label_menu::decorate(const QString& aToken, const bool aWritten)
	{
		static_cast<void>(Controls.apply({ .Change = core::feedback_label_change::keep, .Decoration = core::feedback_decoration_change{ .Token = aToken.toStdString(), .Written = aWritten } }));
		present(Controls.state(), Controls.theme());
	}

	void feedback_label_menu::add_custom()
	{
		const auto token = CustomField->text().trimmed();
		if (token.isEmpty() || !core::valid_feedback_decoration(token.toStdString())) {
			return;
		}
		CustomField->clear();
		decorate(token, true);
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
	auto feedback_label_menu::custom_buttons() const -> std::vector<QToolButton*> { return Custom; }
	auto feedback_label_menu::custom_field() const noexcept -> QLineEdit* { return CustomField; }
	auto feedback_label_menu::custom_add_button() const noexcept -> QToolButton* { return CustomAdd; }
	auto feedback_label_menu::about_button() const noexcept -> QToolButton* { return About; }

	void feedback_label_menu::choose(const core::feedback_header_edit& aEdit)
	{
		static_cast<void>(Controls.apply(aEdit));
		close();
	}

	auto feedback_label_menu::event(QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::ShortcutOverride) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			// ⌘0 is the window's Actual Size shortcut; inside the open menu it
			// is Plain comment, and nowhere else.
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
		// Row 0 is Plain, rows 1 to 4 the grid, row 5 the decorations, row 6
		// the custom ones written (when there are any), row 7 the field and
		// row 8 About Conventional Comments ([[BPR-1566]]), when offered.
		const auto at = [this](const int aRow, const int aColumn) -> QWidget* {
			if (aRow <= 0) {
				return Plain;
			}
			if (aRow <= 4) {
				const auto index = (aRow - 1) * 3 + std::clamp(aColumn, 0, 2);
				return Labels[static_cast<std::size_t>(index)];
			}
			if (aRow == 5) {
				return Decorations[static_cast<std::size_t>(std::clamp(aColumn, 0, 2))];
			}
			if (aRow == 6 && !Custom.empty()) {
				return Custom[static_cast<std::size_t>(std::clamp(aColumn, 0, static_cast<int>(Custom.size()) - 1))];
			}
			if (aRow >= 8) {
				return About;
			}
			return CustomField;
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
		for (std::size_t index = 0U; index < Custom.size(); ++index) {
			if (focusWidget() == Custom[index]) {
				row    = 6;
				column = static_cast<int>(index);
			}
		}
		if (focusWidget() == CustomField || focusWidget() == CustomAdd) {
			row = 7;
		}
		if (focusWidget() == About) {
			row = 8;
		}
		const auto last = About->isHidden() ? 7 : 8;
		const auto step = aRows < 0 ? -1 : 1;
		auto       next = std::clamp(row + aRows, 0, last);
		if (next == 6 && Custom.empty()) {
			next += step;
		}
		auto* target = at(next, column + aColumns);
		// [[BPR-1566]]. On a plain comment the decorations and the field are
		// disabled; an up or down move passes them, so About is reached from
		// the grid and the grid from About. With About hidden this stops
		// where it always did.
		while (aRows != 0 && !target->isEnabled() && next > 0 && next < last) {
			next += step;
			if (next == 6 && Custom.empty()) {
				next += step;
			}
			target = at(next, column + aColumns);
		}
		if (target->isEnabled()) {
			target->setFocus(Qt::TabFocusReason);
		}
	}

	void feedback_label_menu::hideEvent(QHideEvent* const aEvent)
	{
		QFrame::hideEvent(aEvent);
		Controls.return_focus();
	}

	namespace {
		/// Three weights and the if-minor corner on two of them: enough to
		/// show the shapes or the letters, the tints and every style without
		/// becoming a second key.
		constexpr auto preview_entries = std::array{
			feedback_mark_preview::entry{ .Label = core::feedback_label::praise },
			feedback_mark_preview::entry{ .Label = core::feedback_label::suggestion, .Emphasis = { .Weight = feedback_mark_weight::non_blocking, .IfMinor = true } },
			feedback_mark_preview::entry{ .Label = core::feedback_label::issue, .Emphasis = { .Weight = feedback_mark_weight::blocking } },
			feedback_mark_preview::entry{ .Label = core::feedback_label::question },
			feedback_mark_preview::entry{ .Label = core::feedback_label::nitpick, .Emphasis = { .Weight = feedback_mark_weight::non_blocking } },
			feedback_mark_preview::entry{ .Label = core::feedback_label::todo, .Emphasis = { .IfMinor = true } },
		};
		constexpr auto preview_gap = 6;
	} // namespace

	feedback_mark_preview::feedback_mark_preview(QWidget* const aParent)
		: QWidget{ aParent }
		, Theme{ default_feedback_mark_theme(palette()) }
	{
		setObjectName(QStringLiteral("feedbackMarksPreview"));
		setFocusPolicy(Qt::NoFocus);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		name_for_theme();
	}

	void feedback_mark_preview::set_theme(const feedback_mark_theme& aTheme)
	{
		Theme = aTheme;
		name_for_theme();
		update();
	}

	auto feedback_mark_preview::theme() const noexcept -> const feedback_mark_theme&
	{
		return Theme;
	}

	auto feedback_mark_preview::entries() noexcept -> std::span<const entry>
	{
		return preview_entries;
	}

	auto feedback_mark_preview::entry_rect(const std::size_t aIndex) const -> QRectF
	{
		const auto mark = feedback_mark_size;
		const auto left = 1.0 + (static_cast<qreal>(aIndex) * (mark + preview_gap));
		return QRectF{ left, (static_cast<qreal>(height()) - mark) / 2.0, mark, mark };
	}

	auto feedback_mark_preview::entry_at(const QPointF aPoint) const -> std::optional<std::size_t>
	{
		for (std::size_t index = 0U; index < preview_entries.size(); ++index) {
			if (entry_rect(index).contains(aPoint)) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto feedback_mark_preview::entry_tooltip(const entry& aEntry) -> QString
	{
		return feedback_label_text(aEntry.Label) + feedback_mark_emphasis_words(aEntry.Emphasis) + QLatin1Char{ '\n' } + feedback_label_description(aEntry.Label);
	}

	auto feedback_mark_preview::sizeHint() const -> QSize
	{
		const auto mark  = static_cast<int>(feedback_mark_size);
		const auto count = static_cast<int>(preview_entries.size());
		return QSize{ (count * mark) + ((count - 1) * preview_gap) + 2, mark + 2 };
	}

	auto feedback_mark_preview::event(QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::ToolTip) {
			// One tooltip per mark, right below it; the gaps show none.
			const auto* const help = static_cast<QHelpEvent*>(aEvent);
			if (const auto index = entry_at(QPointF{ help->pos() }); index.has_value()) {
				show_anchored_tooltip(*this, entry_rect(*index).toAlignedRect(), entry_tooltip(preview_entries[*index]));
				return true;
			}
			hide_anchored_tooltip();
			aEvent->ignore();
			return true;
		}
		return QWidget::event(aEvent);
	}

	void feedback_mark_preview::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing);
		for (std::size_t index = 0U; index < preview_entries.size(); ++index) {
			const auto& shown = preview_entries[index];
			paint_feedback_mark(painter, entry_rect(index), shown.Label, shown.Emphasis, feedback_mark_colours_for(Theme, shown.Label, palette()));
		}
	}

	void feedback_mark_preview::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			update();
		}
	}

	void feedback_mark_preview::name_for_theme()
	{
		const auto style = Theme.Style == feedback_mark_style::letters ? QStringLiteral("letters") : QStringLiteral("icons");
		setAccessibleName(QStringLiteral("Label marks preview, %1").arg(style));
		auto names = QStringList{ };
		for (const auto& shown : preview_entries) {
			names.push_back(feedback_label_text(shown.Label) + feedback_mark_emphasis_words(shown.Emphasis));
		}
		// The whole list is the accessible description only; the pointer gets
		// one tooltip per mark.
		setAccessibleDescription(QStringLiteral("Marks as threads show them, in %1: %2").arg(style, names.join(QStringLiteral(", "))));
	}
} // namespace bpr::qt
