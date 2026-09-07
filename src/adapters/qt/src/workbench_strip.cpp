#include "bpr/qt/workbench_strip.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QActionEvent>
#include <QColor>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QStyleOption>
#include <QStyleOptionToolButton>
#include <QToolButton>

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief The ink a disabled strip control keeps.
		///
		/// \details The accepted design says nothing in the strip may read as
		/// disabled, which the owner meant as "not washed out". The composed
		/// appearance profile already derives a disabled button text that
		/// keeps a readable contrast on every variant, and the appearance
		/// tests pin that the navigation icons paint exactly that colour, so
		/// the strip uses it rather than inventing a second disabled ink.
		constexpr int soft_fill_alpha = 56;

		[[nodiscard]] auto disabled_ink(const QPalette& aPalette) -> QColor
		{
			return aPalette.color(QPalette::Disabled, QPalette::ButtonText);
		}

		[[nodiscard]] auto soft_fill(QColor aAccent) -> QColor
		{
			aAccent.setAlpha(soft_fill_alpha);
			return aAccent;
		}

		/// \brief Strokes one glyph in a 24-unit box.
		void stroke_glyph(QPainter& aPainter, const strip_glyph aGlyph)
		{
			const auto chevron = [&aPainter](const qreal aX, const bool aLeft) {
				auto path = QPainterPath{ };
				path.moveTo(aLeft ? aX + 5.0 : aX, 6.0);
				path.lineTo(aLeft ? aX : aX + 5.0, 12.0);
				path.lineTo(aLeft ? aX + 5.0 : aX, 18.0);
				aPainter.drawPath(path);
			};
			switch (aGlyph) {
			case strip_glyph::files_drawer:
				aPainter.drawRect(QRectF{ 4.0, 5.0, 16.0, 14.0 });
				aPainter.drawLine(QPointF{ 9.0, 5.0 }, QPointF{ 9.0, 19.0 });
				aPainter.drawLine(QPointF{ 9.0, 10.0 }, QPointF{ 20.0, 10.0 });
				break;
			case strip_glyph::comments_drawer:
				aPainter.drawRect(QRectF{ 4.0, 5.0, 16.0, 14.0 });
				aPainter.drawLine(QPointF{ 15.0, 5.0 }, QPointF{ 15.0, 19.0 });
				break;
			case strip_glyph::previous_change:
				chevron(9.5, true);
				break;
			case strip_glyph::next_change:
				chevron(9.5, false);
				break;
			case strip_glyph::previous_file:
				chevron(6.0, true);
				chevron(13.0, true);
				break;
			case strip_glyph::next_file:
				chevron(6.0, false);
				chevron(13.0, false);
				break;
			case strip_glyph::new_comment: {
				aPainter.setBrush(aPainter.pen().color());
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 4.0, 4.0);
				aPainter.setBrush(Qt::NoBrush);
				break;
			}
			case strip_glyph::refresh: {
				auto arc = QPainterPath{ };
				arc.arcMoveTo(QRectF{ 4.0, 4.0, 16.0, 16.0 }, 180.0);
				arc.arcTo(QRectF{ 4.0, 4.0, 16.0, 16.0 }, 180.0, -300.0);
				aPainter.drawPath(arc);
				auto head = QPainterPath{ };
				head.moveTo(4.0, 4.0);
				head.lineTo(4.0, 9.5);
				head.lineTo(9.5, 9.5);
				aPainter.drawPath(head);
				break;
			}
			case strip_glyph::account: {
				aPainter.drawEllipse(QPointF{ 12.0, 8.0 }, 4.0, 4.0);
				auto shoulders = QPainterPath{ };
				shoulders.moveTo(4.0, 20.0);
				shoulders.cubicTo(5.5, 16.0, 8.5, 14.0, 12.0, 14.0);
				shoulders.cubicTo(15.5, 14.0, 18.5, 16.0, 20.0, 20.0);
				aPainter.drawPath(shoulders);
				break;
			}
			case strip_glyph::settings:
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 3.0, 3.0);
				aPainter.drawLine(QPointF{ 12.0, 2.5 }, QPointF{ 12.0, 5.5 });
				aPainter.drawLine(QPointF{ 12.0, 18.5 }, QPointF{ 12.0, 21.5 });
				aPainter.drawLine(QPointF{ 2.5, 12.0 }, QPointF{ 5.5, 12.0 });
				aPainter.drawLine(QPointF{ 18.5, 12.0 }, QPointF{ 21.5, 12.0 });
				aPainter.drawLine(QPointF{ 5.3, 5.3 }, QPointF{ 7.4, 7.4 });
				aPainter.drawLine(QPointF{ 16.6, 16.6 }, QPointF{ 18.7, 18.7 });
				aPainter.drawLine(QPointF{ 18.7, 5.3 }, QPointF{ 16.6, 7.4 });
				aPainter.drawLine(QPointF{ 7.4, 16.6 }, QPointF{ 5.3, 18.7 });
				break;
			case strip_glyph::review_details:
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 8.5, 8.5);
				aPainter.drawLine(QPointF{ 12.0, 11.0 }, QPointF{ 12.0, 16.5 });
				aPainter.drawLine(QPointF{ 12.0, 7.5 }, QPointF{ 12.0, 8.0 });
				break;
			case strip_glyph::home: {
				auto house = QPainterPath{ };
				house.moveTo(4.0, 11.5);
				house.lineTo(12.0, 4.5);
				house.lineTo(20.0, 11.5);
				house.lineTo(20.0, 20.0);
				house.lineTo(14.5, 20.0);
				house.lineTo(14.5, 14.5);
				house.lineTo(9.5, 14.5);
				house.lineTo(9.5, 20.0);
				house.lineTo(4.0, 20.0);
				house.closeSubpath();
				aPainter.drawPath(house);
				break;
			}
			case strip_glyph::private_progress: {
				auto check = QPainterPath{ };
				check.moveTo(5.0, 12.5);
				check.lineTo(10.0, 17.5);
				check.lineTo(19.0, 7.0);
				aPainter.drawPath(check);
				break;
			}
			}
		}

		[[nodiscard]] auto render_glyph(
			const strip_glyph aGlyph,
			const QColor&     aInk,
			const int         aLogicalSize,
			const double      aScale) -> QPixmap
		{
			const auto pixels = static_cast<int>(
				std::lround(static_cast<double>(aLogicalSize) * aScale));
			auto pixmap = QPixmap{ pixels, pixels };
			pixmap.setDevicePixelRatio(aScale);
			pixmap.fill(Qt::transparent);
			{
				auto painter = QPainter{ &pixmap };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto unit = static_cast<double>(aLogicalSize) / 24.0;
				painter.scale(unit, unit);
				auto pen = QPen{ aInk };
				pen.setWidthF(2.0);
				pen.setCapStyle(Qt::RoundCap);
				pen.setJoinStyle(Qt::RoundJoin);
				painter.setPen(pen);
				painter.setBrush(Qt::NoBrush);
				stroke_glyph(painter, aGlyph);
			}
			return pixmap;
		}
	} // namespace

	auto strip_icon(const strip_glyph aGlyph, const QPalette& aPalette) -> QIcon
	{
		auto           icon         = QIcon{ };
		constexpr auto logicalSizes = std::array{ 16, 20, 24 };
		constexpr auto scales       = std::array{ 1.0, 1.25, 2.0 };
		const auto     ink          = aPalette.color(QPalette::Active, QPalette::ButtonText);
		const auto     modes        = std::array{
			std::pair{ QIcon::Normal, ink },
			std::pair{ QIcon::Active, ink },
			std::pair{ QIcon::Selected, ink },
			std::pair{ QIcon::Disabled, disabled_ink(aPalette) },
		};
		for (const auto& [mode, colour] : modes) {
			for (const auto logicalSize : logicalSizes) {
				for (const auto scale : scales) {
					icon.addPixmap(
						render_glyph(aGlyph, colour, logicalSize, scale),
						mode,
						QIcon::Off);
				}
			}
		}
		return icon;
	}

	strip_style::strip_style() = default;

	void strip_style::drawComplexControl(
		const ComplexControl             aControl,
		const QStyleOptionComplex* const aOption,
		QPainter* const                  aPainter,
		const QWidget* const             aWidget) const
	{
		const auto* const option
			= qstyleoption_cast<const QStyleOptionToolButton*>(aOption);
		if (aControl != CC_ToolButton || option == nullptr || aPainter == nullptr) {
			QProxyStyle::drawComplexControl(aControl, aOption, aPainter, aWidget);
			return;
		}
		const auto& palette = option->palette;
		const auto  enabled = (option->state & State_Enabled) != 0;
		const auto  checked = (option->state & State_On) != 0;
		const auto  hover   = (option->state & State_MouseOver) != 0;
		const auto  down    = (option->state & State_Sunken) != 0;
		const auto  filled  = aWidget != nullptr
			&& aWidget->property(filled_property).toBool();

		// A presenter the toolbar stretched to its row keeps the design's
		// height: the frame is centred in whatever rect the layout gave it.
		auto frame = option->rect;
		if (frame.height() > button_height) {
			frame = QRect{
				frame.left(),
				frame.top() + (frame.height() - button_height) / 2,
				frame.width(),
				button_height,
			};
		}
		const auto box = QRectF{ frame }.adjusted(0.5, 0.5, -0.5, -0.5);

		const auto accent = palette.color(QPalette::Active, QPalette::Highlight);
		auto       fill   = palette.color(QPalette::Active, QPalette::Button);
		auto       border = palette.color(QPalette::Active, QPalette::Mid);
		auto       ink    = palette.color(QPalette::Active, QPalette::ButtonText);
		auto       radius = 5.0;
		if (filled) {
			radius = 4.0;
			border = QColor{ Qt::transparent };
			fill   = checked ? accent : QColor{ Qt::transparent };
			if (checked) {
				ink = palette.color(QPalette::Active, QPalette::HighlightedText);
			}
		} else if (checked) {
			fill   = soft_fill(accent);
			border = accent;
		}
		if (enabled && !(filled && checked)) {
			if (down) {
				fill = palette.color(QPalette::Active, QPalette::Mid);
			} else if (hover) {
				fill = palette.color(QPalette::Active, QPalette::Midlight);
			}
		}
		if (!enabled) {
			ink = disabled_ink(palette);
		}

		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing, true);
		aPainter->setPen(
			border.alpha() == 0 ? QPen{ Qt::NoPen } : QPen{ border, 1.0 });
		aPainter->setBrush(fill);
		aPainter->drawRoundedRect(box, radius, radius);
		if ((option->state & State_HasFocus) != 0) {
			// Visible keyboard focus, drawn inside the border so it never
			// changes the button's footprint.
			aPainter->setPen(QPen{ accent, 1.0 });
			aPainter->setBrush(Qt::NoBrush);
			aPainter->drawRoundedRect(
				box.adjusted(1.5, 1.5, -1.5, -1.5), radius - 1.5, radius - 1.5);
		}

		const auto hasIcon = !option->icon.isNull()
			&& option->toolButtonStyle != Qt::ToolButtonTextOnly;
		const auto hasText = !option->text.isEmpty()
			&& option->toolButtonStyle != Qt::ToolButtonIconOnly;
		const auto menuArrow
			= (option->features & QStyleOptionToolButton::HasMenu) != 0
			|| (option->features & QStyleOptionToolButton::MenuButtonPopup) != 0;
		auto content = frame.adjusted(7, 0, -7, 0);
		if (menuArrow) {
			content.setRight(content.right() - 9);
		}
		const auto iconWidth = hasIcon ? option->iconSize.width() : 0;
		const auto textWidth = hasText
			? option->fontMetrics.horizontalAdvance(option->text)
			: 0;
		const auto gap       = hasIcon && hasText ? 4 : 0;
		const auto total     = iconWidth + gap + textWidth;
		auto       x         = content.left() + (content.width() - total) / 2;
		if (hasIcon) {
			const auto mode     = !enabled ? QIcon::Disabled
				: (down || hover)          ? QIcon::Active
										   : QIcon::Normal;
			const auto iconRect = QRect{
				x,
				frame.top() + (frame.height() - option->iconSize.height()) / 2,
				option->iconSize.width(),
				option->iconSize.height(),
			};
			option->icon.paint(
				aPainter,
				iconRect,
				Qt::AlignCenter,
				mode,
				checked ? QIcon::On : QIcon::Off);
			x += iconWidth + gap;
		}
		if (hasText) {
			aPainter->setPen(ink);
			aPainter->setFont(option->font);
			aPainter->drawText(
				QRect{ x, frame.top(), textWidth + 2, frame.height() },
				Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				option->text);
		}
		if (menuArrow) {
			auto pen = QPen{ ink, 1.5 };
			pen.setCapStyle(Qt::RoundCap);
			pen.setJoinStyle(Qt::RoundJoin);
			aPainter->setPen(pen);
			aPainter->setBrush(Qt::NoBrush);
			const auto right = static_cast<qreal>(frame.right()) - 5.0;
			const auto mid   = static_cast<qreal>(frame.center().y()) + 0.5;
			auto       arrow = QPainterPath{ };
			arrow.moveTo(right - 6.0, mid - 1.5);
			arrow.lineTo(right - 3.0, mid + 1.5);
			arrow.lineTo(right, mid - 1.5);
			aPainter->drawPath(arrow);
		}
		aPainter->restore();
	}

	auto strip_style::sizeFromContents(
		const ContentsType        aType,
		const QStyleOption* const aOption,
		const QSize&              aContentsSize,
		const QWidget* const      aWidget) const -> QSize
	{
		if (aType != CT_ToolButton) {
			return QProxyStyle::sizeFromContents(
				aType, aOption, aContentsSize, aWidget);
		}
		auto width = aContentsSize.width() + 14;
		if (const auto* const option
			= qstyleoption_cast<const QStyleOptionToolButton*>(aOption);
			option != nullptr
			&& ((option->features & QStyleOptionToolButton::HasMenu) != 0
				|| (option->features & QStyleOptionToolButton::MenuButtonPopup)
					!= 0)) {
			width += 9;
		}
		return QSize{ width, button_height };
	}

	auto strip_style::pixelMetric(
		const PixelMetric         aMetric,
		const QStyleOption* const aOption,
		const QWidget* const      aWidget) const -> int
	{
		switch (aMetric) {
		case PM_ToolBarItemSpacing:
			return 4;
		case PM_ToolBarItemMargin:
			// 4 above and below a 22-pixel button is the 30-pixel strip.
			return (workbench_strip::height_pixels - button_height) / 2;
		case PM_ToolBarFrameWidth:
		case PM_ToolBarHandleExtent:
			return 0;
		case PM_ToolBarIconSize:
		case PM_SmallIconSize:
			return 16;
		case PM_ToolBarSeparatorExtent:
			return 13;
		default:
			return QProxyStyle::pixelMetric(aMetric, aOption, aWidget);
		}
	}

	workbench_strip::workbench_strip(const QString& aTitle, QWidget* const aParent)
		: QToolBar{ aTitle, aParent }
		, Style{ new strip_style }
	{
		// Owned as a child so it outlives every presenter that adopted it; a
		// widget's style pointer is guarded, so the order of destruction is
		// not a hazard either way.
		Style->setParent(this);
		setStyle(Style);
		setFixedHeight(height_pixels);
		setIconSize(QSize{ 16, 16 });
		setMovable(false);
		setFloatable(false);
		setToolButtonStyle(Qt::ToolButtonIconOnly);
	}

	workbench_strip::~workbench_strip() = default;

	void workbench_strip::adopt_button(QToolButton* const aButton)
	{
		if (aButton == nullptr) {
			return;
		}
		aButton->setStyle(Style);
		aButton->setFocusPolicy(Qt::StrongFocus);
		aButton->setIconSize(QSize{ 16, 16 });
		aButton->setAutoRaise(false);
	}

	auto workbench_strip::button_style() const noexcept -> strip_style*
	{
		return Style;
	}

	void workbench_strip::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.fillRect(rect(), palette().color(QPalette::Active, QPalette::Window));
		painter.setPen(palette().color(QPalette::Active, QPalette::Mid));
		painter.drawLine(0, height() - 1, width(), height() - 1);
	}

	void workbench_strip::actionEvent(QActionEvent* const aEvent)
	{
		QToolBar::actionEvent(aEvent);
		if (aEvent->type() == QEvent::ActionAdded) {
			adopt_button(qobject_cast<QToolButton*>(
				widgetForAction(aEvent->action())));
		}
	}

	auto workbench_strip::over_empty_space(const QMouseEvent& aEvent) const -> bool
	{
		// Empty means no control: the spacers, the inset, the dividers and
		// the labels are the strip's title-bar surface, so a press on any of
		// them counts, while a press inside a button or another focusable
		// control belongs to that control.
		if (aEvent.button() != Qt::LeftButton) {
			return false;
		}
		for (auto* child = childAt(aEvent.position().toPoint());
			 child != nullptr && child != this;
			 child = child->parentWidget()) {
			if (qobject_cast<QAbstractButton*>(child) != nullptr
				|| child->focusPolicy() != Qt::NoFocus) {
				return false;
			}
		}
		return true;
	}

	void workbench_strip::mousePressEvent(QMouseEvent* const aEvent)
	{
		// [[BPR-0646]]. The strip is the title row: with the platform title
		// bar folded away, empty strip space has to do what that bar did.
		// The strip tracks the drag itself: the hold is where inside the
		// frame the press landed, and every move puts the frame back under
		// it. This is the same on every platform and provable offscreen,
		// where a system move cannot be.
		if (over_empty_space(*aEvent) && window() != nullptr) {
			DragHold = aEvent->globalPosition().toPoint()
				- window()->frameGeometry().topLeft();
			aEvent->accept();
			return;
		}
		QToolBar::mousePressEvent(aEvent);
	}

	void workbench_strip::mouseMoveEvent(QMouseEvent* const aEvent)
	{
		if (DragHold.has_value() && window() != nullptr
			&& (aEvent->buttons() & Qt::LeftButton) != 0) {
			if (window()->isMaximized()) {
				// Dragging a zoomed window out of its zoom, as the platform
				// title bar does: restore first, then follow the pointer.
				window()->showNormal();
			}
			window()->move(aEvent->globalPosition().toPoint() - *DragHold);
			aEvent->accept();
			return;
		}
		QToolBar::mouseMoveEvent(aEvent);
	}

	void workbench_strip::mouseReleaseEvent(QMouseEvent* const aEvent)
	{
		if (DragHold.has_value()) {
			DragHold.reset();
			aEvent->accept();
			return;
		}
		QToolBar::mouseReleaseEvent(aEvent);
	}

	void workbench_strip::mouseDoubleClickEvent(QMouseEvent* const aEvent)
	{
		if (over_empty_space(*aEvent) && window() != nullptr) {
			DragHold.reset();
			if (window()->isMaximized()) {
				window()->showNormal();
			} else {
				window()->showMaximized();
			}
			aEvent->accept();
			return;
		}
		QToolBar::mouseDoubleClickEvent(aEvent);
	}

	strip_divider::strip_divider(QWidget* const aParent)
		: QFrame{ aParent }
	{
		setObjectName(QStringLiteral("workbenchStripDivider"));
		setFrameShape(QFrame::NoFrame);
		setFixedWidth(13);
		setFocusPolicy(Qt::NoFocus);
	}

	void strip_divider::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.fillRect(
			QRect{ width() / 2, (height() - 18) / 2, 1, 18 },
			palette().color(QPalette::Active, QPalette::Mid));
	}

	auto review_modality_title(const review_modality aModality) -> QString
	{
		switch (aModality) {
		case review_modality::overview:
			return QStringLiteral("Overview");
		case review_modality::read:
			return QStringLiteral("Read");
		case review_modality::review:
			return QStringLiteral("Review");
		case review_modality::discuss:
			return QStringLiteral("Discuss");
		}
		return QStringLiteral("Review");
	}

	namespace {
		[[nodiscard]] auto modality_purpose(const review_modality aModality)
			-> QString
		{
			switch (aModality) {
			case review_modality::overview:
				return QStringLiteral(
					"Open the files drawer and close the comments drawer to read "
					"the pull request overview.");
			case review_modality::read:
				return QStringLiteral(
					"Collapse the files drawer and keep comments beside the code.");
			case review_modality::review:
				return QStringLiteral("Open the files and comments drawers.");
			case review_modality::discuss:
				return QStringLiteral(
					"Collapse the files drawer and give the conversation the room.");
			}
			return QString{ };
		}
	} // namespace

	modality_control::modality_control(workbench_strip& aStrip, QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchModalityControl"));
		setAccessibleName(QStringLiteral("Review modality"));
		setAccessibleDescription(QStringLiteral(
			"Overview, Read, Review and Discuss presets for which drawers are open"));
		setFocusPolicy(Qt::NoFocus);
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(2, 2, 2, 2);
		row->setSpacing(2);
		for (std::size_t index = 0U; index < review_modality_count; ++index) {
			const auto  modality = static_cast<review_modality>(index);
			const auto  title    = review_modality_title(modality);
			auto* const action   = new QAction{ title, this };
			action->setObjectName(
				QStringLiteral("reviewModality%1Action").arg(title));
			action->setCheckable(true);
			action->setShortcut(
				QKeySequence{ QStringLiteral("Ctrl+%1").arg(index + 1U) });
			action->setShortcutContext(Qt::WindowShortcut);
			action->setToolTip(modality_purpose(modality));
			action->setStatusTip(action->toolTip());
			action->setIconText(title);
			// The button is not the action's default presenter: a presenter
			// re-reads its text from the action on every checked change, which
			// would drop the shortcut the segment shows beside its name. The
			// two are kept in step by hand instead.
			auto* const button = new QToolButton{ this };
			button->setObjectName(
				QStringLiteral("reviewModality%1Button").arg(title));
			// The shortcut lives on the control, not the button: an action
			// added to a tool button makes it a menu button with an arrow.
			addAction(action);
			button->setCheckable(true);
			button->setText(title);
			button->setToolTip(modality_purpose(modality));
			button->setStatusTip(button->toolTip());
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			button->setProperty(strip_style::filled_property, true);
			button->setAccessibleName(QStringLiteral("%1 modality").arg(title));
			button->setAccessibleDescription(modality_purpose(modality));
			aStrip.adopt_button(button);
			button->setFixedHeight(strip_style::button_height - 4);
			connect(button, &QToolButton::clicked, this, [this, modality] {
				choose(modality);
			});
			connect(action, &QAction::triggered, this, [this, modality] {
				choose(modality);
			});
			Actions[index] = action;
			Buttons[index] = button;
			row->addWidget(button);
		}
		set_compact(false);
		adopt(Current);
	}

	void modality_control::set_compact(const bool aCompact)
	{
		Compact = aCompact;
		for (std::size_t index = 0U; index < review_modality_count; ++index) {
			const auto title = review_modality_title(static_cast<review_modality>(index));
			Buttons[index]->setText(
				aCompact ? title
						 : QStringLiteral("%1  %2").arg(
							   title,
							   Actions[index]->shortcut().toString(
								   QKeySequence::NativeText)));
		}
		updateGeometry();
	}

	auto modality_control::compact() const noexcept -> bool
	{
		return Compact;
	}

	void modality_control::set_dispatch(review_modality_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void modality_control::adopt(const review_modality aModality)
	{
		Current = aModality;
		for (std::size_t index = 0U; index < review_modality_count; ++index) {
			const auto current = static_cast<review_modality>(index) == aModality;
			Actions[index]->setChecked(current);
			Buttons[index]->setChecked(current);
		}
		update();
	}

	auto modality_control::current() const noexcept -> review_modality
	{
		return Current;
	}

	auto modality_control::action(const review_modality aModality) const noexcept
		-> QAction*
	{
		return Actions[static_cast<std::size_t>(aModality)];
	}

	auto modality_control::button(const review_modality aModality) const noexcept
		-> QToolButton*
	{
		return Buttons[static_cast<std::size_t>(aModality)];
	}

	void modality_control::choose(const review_modality aModality)
	{
		// A checkable action flips itself when triggered; the control shows
		// the chosen modality whatever the click did to the others.
		adopt(aModality);
		if (Dispatch) {
			Dispatch(aModality);
		}
	}

	void modality_control::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
		painter.drawRoundedRect(
			QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
	}
} // namespace bpr::qt
