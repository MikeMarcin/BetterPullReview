#include "bpr/qt/workbench_strip.hpp"

#include "bpr/qt/epoch_line.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"

#include "painted_glyph_icon.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QActionEvent>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionToolButton>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief [[BPR-1129]]. The amber of my last review's tick on the line,
		/// which a button's mark repeats.
		[[nodiscard]] auto mark_colour() -> QColor
		{
			return QColor{ 0xD2, 0x99, 0x22 };
		}

		/// \brief [[BPR-1129]]. The pill a badge count is drawn in.
		constexpr int badge_height  = 14;
		constexpr int badge_padding = 4;
		constexpr int badge_gap     = 4;

		[[nodiscard]] auto badge_text(const QWidget* const aWidget) -> QString
		{
			return aWidget == nullptr ? QString{ } : aWidget->property(strip_style::badge_property).toString();
		}

		[[nodiscard]] auto badge_width(const QFontMetrics& aMetrics, const QString& aText) -> int
		{
			return aText.isEmpty() ? 0 : std::max(badge_height, aMetrics.horizontalAdvance(aText) + 2 * badge_padding);
		}

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

		/// \brief [[BPR-1331]]. Draws one private progress mark in a 24-unit box.
		///
		/// \details One 16-unit circle in every state, so the button keeps its
		/// place and only the fill moves: broken ring, half disc, disc, disc
		/// with a tick cut out, ring struck through. The shapes carry the state
		/// without colour, as the Overview's read dots do.
		void stroke_progress_mark(QPainter& aPainter, const strip_glyph aGlyph)
		{
			constexpr auto radius = 7.0;
			const auto     centre = QPointF{ 12.0, 12.0 };
			const auto     ink    = aPainter.pen().color();
			if (aGlyph == strip_glyph::progress_unread) {
				// Eight dashes round the ring: 3.2 units drawn, 2.3 left open.
				auto pen = aPainter.pen();
				pen.setCapStyle(Qt::FlatCap);
				pen.setDashPattern({ 1.6, 1.15 });
				aPainter.setPen(pen);
				aPainter.drawEllipse(centre, radius, radius);
			} else if (aGlyph == strip_glyph::progress_seen) {
				aPainter.save();
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(ink);
				aPainter.drawPie(QRectF{ centre.x() - radius, centre.y() - radius, 2.0 * radius, 2.0 * radius }, 90 * 16, 180 * 16);
				aPainter.restore();
				aPainter.drawEllipse(centre, radius, radius);
			} else if (aGlyph == strip_glyph::progress_read) {
				aPainter.setBrush(ink);
				aPainter.drawEllipse(centre, radius, radius);
				aPainter.setBrush(Qt::NoBrush);
			} else if (aGlyph == strip_glyph::progress_accepted) {
				auto disc = QPainterPath{ };
				disc.addEllipse(centre, radius + 1.0, radius + 1.0);
				auto tick = QPainterPath{ };
				tick.moveTo(8.0, 12.2);
				tick.lineTo(11.0, 15.2);
				tick.lineTo(16.2, 9.0);
				auto cut = QPainterPathStroker{ };
				cut.setWidth(2.2);
				cut.setCapStyle(Qt::RoundCap);
				cut.setJoinStyle(Qt::RoundJoin);
				aPainter.fillPath(disc.subtracted(cut.createStroke(tick)), ink);
			} else if (aGlyph == strip_glyph::progress_excluded) {
				aPainter.drawEllipse(centre, radius, radius);
				const auto reach = radius * std::numbers::sqrt2 / 2.0;
				aPainter.drawLine(QPointF{ centre.x() - reach, centre.y() + reach }, QPointF{ centre.x() + reach, centre.y() - reach });
			}
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
			case strip_glyph::repositories_drawer:
				// A panel with a tree in it: a root row and two indented rows.
				aPainter.drawRect(QRectF{ 4.0, 5.0, 16.0, 14.0 });
				aPainter.drawLine(QPointF{ 7.0, 9.0 }, QPointF{ 12.0, 9.0 });
				aPainter.drawLine(QPointF{ 9.0, 12.5 }, QPointF{ 14.0, 12.5 });
				aPainter.drawLine(QPointF{ 9.0, 16.0 }, QPointF{ 14.0, 16.0 });
				aPainter.drawLine(QPointF{ 8.0, 9.0 }, QPointF{ 8.0, 16.0 });
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
			case strip_glyph::progress_unread:
			case strip_glyph::progress_seen:
			case strip_glyph::progress_read:
			case strip_glyph::progress_accepted:
			case strip_glyph::progress_excluded:
				stroke_progress_mark(aPainter, aGlyph);
				break;
			case strip_glyph::compose_review: {
				// A pencil leaning right, its point at the lower left.
				auto pencil = QPainterPath{ };
				pencil.moveTo(5.0, 19.0);
				pencil.lineTo(6.0, 15.0);
				pencil.lineTo(15.5, 5.5);
				pencil.lineTo(18.5, 8.5);
				pencil.lineTo(9.0, 18.0);
				pencil.closeSubpath();
				aPainter.drawPath(pencil);
				aPainter.drawLine(QPointF{ 13.5, 7.5 }, QPointF{ 16.5, 10.5 });
				break;
			}
			case strip_glyph::drawers_shown:
			case strip_glyph::drawers_hidden: {
				// A window with a column at each side. Showing, the columns are
				// filled: the drawers take that room. Hidden, they are outlines
				// and the code has the window.
				aPainter.drawRect(QRectF{ 3.0, 5.0, 18.0, 14.0 });
				aPainter.drawLine(QPointF{ 8.0, 5.0 }, QPointF{ 8.0, 19.0 });
				aPainter.drawLine(QPointF{ 16.0, 5.0 }, QPointF{ 16.0, 19.0 });
				if (aGlyph == strip_glyph::drawers_shown) {
					const auto ink = aPainter.pen().color();
					aPainter.fillRect(QRectF{ 3.0, 5.0, 5.0, 14.0 }, ink);
					aPainter.fillRect(QRectF{ 16.0, 5.0, 5.0, 14.0 }, ink);
				}
				break;
			}
			case strip_glyph::comment: {
				// A speech balloon with its tail at the lower left.
				auto balloon = QPainterPath{ };
				balloon.moveTo(6.0, 5.0);
				balloon.lineTo(18.0, 5.0);
				balloon.quadTo(20.0, 5.0, 20.0, 7.0);
				balloon.lineTo(20.0, 14.0);
				balloon.quadTo(20.0, 16.0, 18.0, 16.0);
				balloon.lineTo(11.0, 16.0);
				balloon.lineTo(7.0, 19.5);
				balloon.lineTo(7.0, 16.0);
				balloon.lineTo(6.0, 16.0);
				balloon.quadTo(4.0, 16.0, 4.0, 14.0);
				balloon.lineTo(4.0, 7.0);
				balloon.quadTo(4.0, 5.0, 6.0, 5.0);
				aPainter.drawPath(balloon);
				break;
			}
			}
		}

		/// Strokes `aGlyph` `aSide` logical pixels square at the origin in `aInk`.
		void paint_glyph(QPainter& aPainter, const strip_glyph aGlyph, const qreal aSide, const QColor& aInk)
		{
			aPainter.save();
			const auto unit = aSide / 24.0;
			aPainter.scale(unit, unit);
			auto pen = QPen{ aInk };
			pen.setWidthF(2.0);
			pen.setCapStyle(Qt::RoundCap);
			pen.setJoinStyle(Qt::RoundJoin);
			aPainter.setPen(pen);
			aPainter.setBrush(Qt::NoBrush);
			stroke_glyph(aPainter, aGlyph);
			aPainter.restore();
		}
	} // namespace

	auto progress_glyph(const std::optional<core::file_progress_mark> aMark) noexcept -> strip_glyph
	{
		if (!aMark.has_value()) {
			return strip_glyph::private_progress;
		}
		switch (*aMark) {
		case core::file_progress_mark::unread:
			return strip_glyph::progress_unread;
		case core::file_progress_mark::seen:
			return strip_glyph::progress_seen;
		case core::file_progress_mark::read:
			return strip_glyph::progress_read;
		case core::file_progress_mark::accepted:
			return strip_glyph::progress_accepted;
		case core::file_progress_mark::excluded:
			return strip_glyph::progress_excluded;
		}
		return strip_glyph::private_progress;
	}

	auto strip_icon(const strip_glyph aGlyph, const QPalette& aPalette) -> QIcon
	{
		// [[BPR-1296]]. Painted at the size and scale the button asks for.
		return painted_glyph_icon([aGlyph](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_glyph(aPainter, aGlyph, aSide, aInk); },
								  aPalette.color(QPalette::Active, QPalette::ButtonText), disabled_ink(aPalette));
	}

	void set_strip_badge(QToolButton& aButton, const QString& aBadge)
	{
		if (aButton.property(strip_style::badge_property).toString() == aBadge) {
			return;
		}
		aButton.setProperty(strip_style::badge_property, aBadge);
		// A button caches its size hint until one of its change events clears
		// it; the badge is part of the size, so a style change is sent.
		auto changed = QEvent{ QEvent::StyleChange };
		QCoreApplication::sendEvent(&aButton, &changed);
		aButton.updateGeometry();
		aButton.update();
	}

	strip_style::strip_style(const int aRowHeight)
		: RowHeight{ aRowHeight }
	{
	}

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
			// [[BPR-1157]]. Keyboard focus is the window's focus ring: two
			// pixels over the border, so it never changes the button's
			// footprint, and distinct from a checked button's one-pixel
			// accent border. The owner could not tell what had focus when it
			// was one pixel of the accent inside the border.
			aPainter->setPen(QPen{ focus_ring_colour(palette, QPalette::Window), focus_ring_width });
			aPainter->setBrush(Qt::NoBrush);
			const auto inset = (focus_ring_width - 1.0) / 2.0;
			aPainter->drawRoundedRect(box.adjusted(inset, inset, -inset, -inset), radius - inset, radius - inset);
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
		const auto badge     = badge_text(aWidget);
		const auto pill      = badge_width(option->fontMetrics, badge);
		const auto total     = iconWidth + gap + textWidth + (pill > 0 ? badge_gap + pill : 0);
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
			if (aWidget != nullptr && aWidget->property(mark_property).toBool()) {
				// [[BPR-1129]]. A kept draft: an amber dot on the icon's upper right.
				aPainter->setPen(QPen{ palette.color(QPalette::Active, QPalette::Button), 1.0 });
				aPainter->setBrush(mark_colour());
				aPainter->drawEllipse(QPointF{ static_cast<qreal>(iconRect.right()) - 0.5, static_cast<qreal>(iconRect.top()) + 1.5 }, 3.5, 3.5);
			}
			x += iconWidth + gap;
		}
		if (hasText) {
			aPainter->setPen(ink);
			aPainter->setFont(option->font);
			aPainter->drawText(
				QRect{ x, frame.top(), textWidth + 2, frame.height() },
				Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				option->text);
			x += textWidth;
		}
		if (pill > 0) {
			// [[BPR-1129]]. The count the button carries, in a pill beside its icon.
			x += badge_gap;
			const auto pillRect = QRectF{
				static_cast<qreal>(x),
				static_cast<qreal>(frame.top()) + static_cast<qreal>(frame.height() - badge_height) / 2.0,
				static_cast<qreal>(pill),
				static_cast<qreal>(badge_height),
			};
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(accent);
			aPainter->drawRoundedRect(pillRect, badge_height / 2.0, badge_height / 2.0);
			aPainter->setPen(palette.color(QPalette::Active, QPalette::HighlightedText));
			aPainter->setFont(option->font);
			aPainter->drawText(pillRect, Qt::AlignCenter | Qt::TextSingleLine, badge);
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
		auto              width = aContentsSize.width() + 14;
		const auto* const option
			= qstyleoption_cast<const QStyleOptionToolButton*>(aOption);
		if (option != nullptr
			&& ((option->features & QStyleOptionToolButton::HasMenu) != 0
				|| (option->features & QStyleOptionToolButton::MenuButtonPopup)
					!= 0)) {
			width += 9;
		}
		if (const auto badge = badge_text(aWidget); !badge.isEmpty() && option != nullptr) {
			width += badge_gap + badge_width(option->fontMetrics, badge);
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
			// 4 above and below a 22-pixel button is the 30-pixel strip; 3 is
			// the 28-pixel review bar.
			return (RowHeight - button_height) / 2;
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

	workbench_strip::workbench_strip(const QString& aTitle, QWidget* const aParent, const strip_role aRole)
		: QToolBar{ aTitle, aParent }
		, Style{ new strip_style{ aRole == strip_role::title_row ? height_pixels : review_bar_height_pixels } }
		, Role{ aRole }
	{
		// Owned as a child so it outlives every presenter that adopted it; a
		// widget's style pointer is guarded, so the order of destruction is
		// not a hazard either way.
		Style->setParent(this);
		setStyle(Style);
		setFixedHeight(aRole == strip_role::title_row ? height_pixels : review_bar_height_pixels);
		setIconSize(QSize{ 16, 16 });
		setMovable(false);
		setFloatable(false);
		setToolButtonStyle(Qt::ToolButtonIconOnly);
	}

	workbench_strip::~workbench_strip() = default;

	auto workbench_strip::role() const noexcept -> strip_role
	{
		return Role;
	}

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

	void workbench_strip::set_navigation_rails(QAction* const aFileLabel, QAction* const aFileLast, QAction* const aChangeLabel, QAction* const aChangeLast)
	{
		NavigationRails = { aFileLabel, aFileLast, aChangeLabel, aChangeLast };
		update();
	}

	void workbench_strip::paint_navigation_rails(QPainter& aPainter) const
	{
		for (const auto index : { 0U, 2U }) {
			if (NavigationRails[index] == nullptr || NavigationRails[index + 1U] == nullptr) {
				continue;
			}
			const auto first = actionGeometry(NavigationRails[index]);
			const auto last  = actionGeometry(NavigationRails[index + 1U]);
			if (!first.isEmpty() && !last.isEmpty() && first.left() < last.right()) {
				aPainter.fillRect(QRect{ first.left() + 2, height() - 3, last.right() - first.left() - 3, 2 }, palette().color(QPalette::Active, QPalette::Highlight));
			}
		}
	}

	bool workbench_strip::event(QEvent* const aEvent)
	{
		const auto handled = QToolBar::event(aEvent);
		// Label and comparison text can move child controls without resizing
		// the toolbar. Repaint the rails after that layout has been serviced.
		if (aEvent->type() == QEvent::LayoutRequest) {
			update();
		}
		return handled;
	}

	void workbench_strip::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.fillRect(rect(), palette().color(QPalette::Active, QPalette::Window));
		painter.setPen(palette().color(QPalette::Active, QPalette::Mid));
		painter.drawLine(0, height() - 1, width(), height() - 1);
		paint_navigation_rails(painter);
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
		// [[BPR-1129]]. Only the title row stands in for the title bar.
		return Role == strip_role::title_row && aEvent.button() == Qt::LeftButton
			&& empty_surface_at(aEvent.position().toPoint());
	}

	auto workbench_strip::empty_surface_at(const QPoint aPoint) const -> bool
	{
		// Empty means no control: the spacers, the inset, the dividers and
		// the labels are the strip's title-bar surface, so a press on any of
		// them counts, while a press inside a button or another focusable
		// control belongs to that control.
		if (!rect().contains(aPoint)) {
			return false;
		}
		for (auto* child = childAt(aPoint);
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
		case review_modality::review:
			return QStringLiteral("Review");
		case review_modality::discuss:
			return QStringLiteral("Discuss");
		case review_modality::conversations:
			return QStringLiteral("Conversations");
		}
		return QStringLiteral("Review");
	}

	auto review_modality_shortcut(const review_modality aModality) -> QKeySequence
	{
		switch (aModality) {
		case review_modality::overview:
			return QKeySequence{ QStringLiteral("Ctrl+1") };
		case review_modality::review:
			return QKeySequence{ QStringLiteral("Ctrl+2") };
		case review_modality::discuss:
			return QKeySequence{ QStringLiteral("Ctrl+3") };
		case review_modality::conversations:
			return QKeySequence{ QStringLiteral("Ctrl+4") };
		}
		return QKeySequence{ };
	}

	namespace {
		[[nodiscard]] auto modality_purpose(const review_modality aModality)
			-> QString
		{
			switch (aModality) {
			case review_modality::overview:
				// [[BPR-1295]]. It said Overview opened the files drawer, but
				// Overview has closed every drawer since [[BPR-0674]].
				return QStringLiteral(
					"Read the pull request overview, with every drawer closed.");
			case review_modality::review:
				return QStringLiteral(
					"Review the code in the drawers you last arranged. Choose "
					"Review again to open Files and Comments.");
			case review_modality::discuss:
				return QStringLiteral(
					"Collapse the files drawer and give the conversation the room.");
			case review_modality::conversations:
				// [[BPR-1293]]. The card's words, with both keys named.
				return QStringLiteral("Find a conversation in the pull request (%1, %2)")
					.arg(review_modality_shortcut(review_modality::conversations).toString(QKeySequence::NativeText),
						 QKeySequence{ QStringLiteral("Ctrl+J") }.toString(QKeySequence::NativeText));
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
			"Overview, Review and Discuss presets for which drawers are open, "
			"and Conversations, which finds a conversation in the pull request"));
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
			action->setShortcut(review_modality_shortcut(modality));
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
		relabel();
	}

	auto modality_control::compact() const noexcept -> bool
	{
		return Compact;
	}

	void modality_control::set_abbreviated(const bool aAbbreviated)
	{
		Abbreviated = aAbbreviated;
		relabel();
	}

	auto modality_control::abbreviated() const noexcept -> bool
	{
		return Abbreviated;
	}

	void modality_control::relabel()
	{
		for (std::size_t index = 0U; index < review_modality_count; ++index) {
			const auto title = review_modality_title(static_cast<review_modality>(index));
			// [[BPR-0653]]. The three-character prefixes are distinct, and the
			// button's accessible name and tooltip still carry the whole word.
			Buttons[index]->setText(
				Abbreviated   ? title.left(3)
					: Compact ? title
							  : QStringLiteral("%1  %2").arg(
									title,
									Actions[index]->shortcut().toString(
										QKeySequence::NativeText)));
		}
		updateGeometry();
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

	namespace {
		/// \brief [[BPR-1130]]. The disclosure's geometry: an inset, the
		/// triangle, a gap, the name, and the mark after it when there is one.
		constexpr int disclosure_inset    = 5;
		constexpr int disclosure_triangle = 7;
		constexpr int disclosure_gap      = 5;
		constexpr int disclosure_mark_gap = 5;
		constexpr int disclosure_height   = 22;

		/// \brief The design draws "Updates" and its triangle in a quieter ink
		/// than the comparison it names; three quarters of the text colour over
		/// the row keeps it well above the contrast a label needs.
		[[nodiscard]] auto disclosure_ink(const QPalette& aPalette, const bool aEnabled) -> QColor
		{
			const auto group = aEnabled ? QPalette::Active : QPalette::Disabled;
			const auto text  = aPalette.color(group, QPalette::WindowText);
			const auto base  = aPalette.color(group, QPalette::Window);
			return QColor::fromRgbF(
				static_cast<float>((text.redF() * 0.75) + (base.redF() * 0.25)),
				static_cast<float>((text.greenF() * 0.75) + (base.greenF() * 0.25)),
				static_cast<float>((text.blueF() * 0.75) + (base.blueF() * 0.25)));
		}
	} // namespace

	updates_disclosure::updates_disclosure(QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setObjectName(QStringLiteral("reviewBarUpdates"));
		setCheckable(true);
		setFocusPolicy(Qt::StrongFocus);
		setToolButtonStyle(Qt::ToolButtonTextOnly);
		setAttribute(Qt::WA_Hover, true);
		setAccessibleName(QStringLiteral("Updates"));
	}

	void updates_disclosure::set_open(const bool aOpen)
	{
		Open = aOpen;
		setArrowType(aOpen ? Qt::DownArrow : Qt::RightArrow);
		setChecked(aOpen);
		update();
	}

	auto updates_disclosure::open() const noexcept -> bool
	{
		return Open;
	}

	void updates_disclosure::set_history_marked(const bool aMarked)
	{
		if (Marked == aMarked) {
			return;
		}
		Marked = aMarked;
		updateGeometry();
		update();
	}

	auto updates_disclosure::history_marked() const noexcept -> bool
	{
		return Marked;
	}

	auto updates_disclosure::sizeHint() const -> QSize
	{
		auto width = disclosure_inset + disclosure_triangle + disclosure_gap
			+ fontMetrics().horizontalAdvance(text()) + disclosure_inset;
		if (Marked) {
			width += disclosure_mark_gap + epoch_line::history_mark_size;
		}
		return QSize{ width, std::max(disclosure_height, fontMetrics().height() + 6) };
	}

	auto updates_disclosure::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void updates_disclosure::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		// Painted whole: the review bar frames its presenters, and a
		// disclosure is a line of text with its triangle, not a button. The
		// arrow says the state, so a checked highlight would say it twice.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  enabled = isEnabled();
		const auto  frame   = QRect{ 0, (height() - disclosure_height) / 2, width(), disclosure_height };
		if (enabled && (isDown() || underMouse())) {
			auto hover = colours.color(QPalette::Active, QPalette::Mid);
			hover.setAlpha(isDown() ? 110 : 60);
			painter.setPen(Qt::NoPen);
			painter.setBrush(hover);
			painter.drawRoundedRect(QRectF{ frame }.adjusted(0.5, 0.5, -0.5, -0.5), 5.0, 5.0);
		}
		const auto ink    = disclosure_ink(colours, enabled);
		const auto middle = static_cast<qreal>(frame.center().y()) + 0.5;
		const auto left   = static_cast<qreal>(disclosure_inset);
		const auto side   = static_cast<qreal>(disclosure_triangle);
		auto       arrow  = QPainterPath{ };
		if (Open) {
			arrow.moveTo(left, middle - (side * 0.3));
			arrow.lineTo(left + side, middle - (side * 0.3));
			arrow.lineTo(left + (side / 2.0), middle + (side * 0.45));
		} else {
			arrow.moveTo(left + (side * 0.15), middle - (side / 2.0));
			arrow.lineTo(left + (side * 0.9), middle);
			arrow.lineTo(left + (side * 0.15), middle + (side / 2.0));
		}
		arrow.closeSubpath();
		painter.setPen(Qt::NoPen);
		painter.setBrush(ink);
		painter.drawPath(arrow);
		const auto textLeft  = disclosure_inset + disclosure_triangle + disclosure_gap;
		const auto textWidth = fontMetrics().horizontalAdvance(text());
		painter.setPen(ink);
		painter.drawText(QRect{ textLeft, frame.top(), textWidth + 1, frame.height() }, Qt::AlignLeft | Qt::AlignVCenter, text());
		if (Marked) {
			epoch_line::paint_history_mark(
				painter,
				QRect{ textLeft + textWidth + disclosure_mark_gap, frame.top(), epoch_line::history_mark_size, frame.height() },
				colours);
		}
		if (hasFocus()) {
			// [[BPR-1157]]. The window's focus ring, as the review bar's buttons
			// draw it: a style's focus rectangle is nothing at all on macOS.
			constexpr auto inset = focus_ring_width / 2.0;
			painter.setPen(QPen{ focus_ring_colour(colours, QPalette::Window), focus_ring_width });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ frame }.adjusted(inset, inset, -inset, -inset), 5.0 - inset, 5.0 - inset);
		}
	}
} // namespace bpr::qt
