#include "bpr/qt/review_cycle_share_marks.hpp"

#include "painted_glyph_icon.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>

#include <array>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		/// Draws `aGlyph` on a 24-unit grid with the painter's pen.
		void stroke_glyph(QPainter& aPainter, const share_glyph aGlyph)
		{
			const auto dot = [&aPainter](const QPointF aAt) {
				const auto pen = aPainter.pen();
				aPainter.save();
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(pen.color());
				aPainter.drawEllipse(aAt, 1.4, 1.4);
				aPainter.restore();
			};
			const auto ring = [&aPainter] { aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 9.0, 9.0); };
			switch (aGlyph) {
			case share_glyph::copy: {
				aPainter.drawRoundedRect(QRectF{ 9.0, 9.0, 11.0, 11.0 }, 2.0, 2.0);
				auto back = QPainterPath{ };
				back.moveTo(5.0, 15.0);
				back.lineTo(5.0, 6.5);
				back.quadTo(5.0, 5.0, 6.5, 5.0);
				back.lineTo(15.0, 5.0);
				aPainter.drawPath(back);
				break;
			}
			case share_glyph::save:
				aPainter.drawLine(QPointF{ 12.0, 4.0 }, QPointF{ 12.0, 15.0 });
				aPainter.drawPolyline(std::array{ QPointF{ 7.5, 10.5 }, QPointF{ 12.0, 15.0 }, QPointF{ 16.5, 10.5 } }.data(), 3);
				aPainter.drawPolyline(std::array{ QPointF{ 4.0, 15.0 }, QPointF{ 4.0, 20.0 }, QPointF{ 20.0, 20.0 }, QPointF{ 20.0, 15.0 } }.data(), 4);
				break;
			case share_glyph::bundle:
				aPainter.drawRect(QRectF{ 3.5, 4.0, 17.0, 4.5 });
				aPainter.drawPolyline(std::array{ QPointF{ 5.0, 8.5 }, QPointF{ 5.0, 20.0 }, QPointF{ 19.0, 20.0 }, QPointF{ 19.0, 8.5 } }.data(), 4);
				aPainter.drawLine(QPointF{ 10.0, 12.5 }, QPointF{ 14.0, 12.5 });
				break;
			case share_glyph::page: {
				auto page = QPainterPath{ };
				page.moveTo(6.0, 3.0);
				page.lineTo(14.0, 3.0);
				page.lineTo(19.0, 8.0);
				page.lineTo(19.0, 21.0);
				page.lineTo(6.0, 21.0);
				page.closeSubpath();
				aPainter.drawPath(page);
				aPainter.drawLine(QPointF{ 9.0, 12.0 }, QPointF{ 16.0, 12.0 });
				aPainter.drawLine(QPointF{ 9.0, 16.0 }, QPointF{ 16.0, 16.0 });
				break;
			}
			case share_glyph::terminal:
				aPainter.drawRoundedRect(QRectF{ 3.0, 5.0, 18.0, 14.0 }, 2.0, 2.0);
				aPainter.drawPolyline(std::array{ QPointF{ 7.0, 9.5 }, QPointF{ 10.0, 12.0 }, QPointF{ 7.0, 14.5 } }.data(), 3);
				aPainter.drawLine(QPointF{ 12.0, 15.0 }, QPointF{ 17.0, 15.0 });
				break;
			case share_glyph::send: {
				auto plane = QPainterPath{ };
				plane.moveTo(3.0, 11.0);
				plane.lineTo(21.0, 3.0);
				plane.lineTo(14.0, 21.0);
				plane.lineTo(11.0, 13.0);
				plane.closeSubpath();
				aPainter.drawPath(plane);
				aPainter.drawLine(QPointF{ 11.0, 13.0 }, QPointF{ 21.0, 3.0 });
				break;
			}
			case share_glyph::copied:
				aPainter.drawRoundedRect(QRectF{ 5.0, 5.0, 14.0, 16.0 }, 2.0, 2.0);
				aPainter.drawRoundedRect(QRectF{ 9.0, 3.0, 6.0, 4.0 }, 1.0, 1.0);
				aPainter.drawPolyline(std::array{ QPointF{ 8.5, 13.5 }, QPointF{ 11.0, 16.0 }, QPointF{ 15.5, 11.0 } }.data(), 3);
				break;
			case share_glyph::read: {
				auto eye = QPainterPath{ };
				eye.moveTo(2.5, 12.0);
				eye.cubicTo(6.5, 5.5, 17.5, 5.5, 21.5, 12.0);
				eye.cubicTo(17.5, 18.5, 6.5, 18.5, 2.5, 12.0);
				aPainter.drawPath(eye);
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 3.0, 3.0);
				break;
			}
			case share_glyph::handoff:
				aPainter.drawRoundedRect(QRectF{ 3.0, 4.0, 18.0, 16.0 }, 2.0, 2.0);
				aPainter.drawPolyline(std::array{ QPointF{ 3.0, 13.0 }, QPointF{ 8.0, 13.0 }, QPointF{ 9.5, 16.0 }, QPointF{ 14.5, 16.0 }, QPointF{ 16.0, 13.0 }, QPointF{ 21.0, 13.0 } }.data(), 6);
				break;
			case share_glyph::acknowledged: {
				auto bubble = QPainterPath{ };
				bubble.moveTo(4.0, 5.0);
				bubble.lineTo(20.0, 5.0);
				bubble.lineTo(20.0, 16.0);
				bubble.lineTo(10.0, 16.0);
				bubble.lineTo(6.0, 20.0);
				bubble.lineTo(6.0, 16.0);
				bubble.lineTo(4.0, 16.0);
				bubble.closeSubpath();
				aPainter.drawPath(bubble);
				aPainter.drawPolyline(std::array{ QPointF{ 8.5, 10.5 }, QPointF{ 11.0, 13.0 }, QPointF{ 15.5, 8.0 } }.data(), 3);
				break;
			}
			case share_glyph::uncertain: {
				ring();
				auto mark = QPainterPath{ };
				mark.moveTo(9.5, 9.5);
				mark.cubicTo(9.5, 6.5, 14.5, 6.5, 14.5, 9.5);
				mark.cubicTo(14.5, 11.5, 12.0, 11.5, 12.0, 13.8);
				aPainter.drawPath(mark);
				dot(QPointF{ 12.0, 17.0 });
				break;
			}
			case share_glyph::failed:
				ring();
				aPainter.drawLine(QPointF{ 9.0, 9.0 }, QPointF{ 15.0, 15.0 });
				aPainter.drawLine(QPointF{ 15.0, 9.0 }, QPointF{ 9.0, 15.0 });
				break;
			case share_glyph::refused:
				ring();
				aPainter.drawLine(QPointF{ 5.8, 18.2 }, QPointF{ 18.2, 5.8 });
				break;
			case share_glyph::pending: {
				auto pen = aPainter.pen();
				pen.setDashPattern({ 1.6, 1.9 });
				pen.setCapStyle(Qt::FlatCap);
				aPainter.save();
				aPainter.setPen(pen);
				ring();
				aPainter.restore();
				dot(QPointF{ 12.0, 12.0 });
				break;
			}
			case share_glyph::changed: {
				auto pencil = QPainterPath{ };
				pencil.moveTo(4.0, 20.0);
				pencil.lineTo(5.0, 15.0);
				pencil.lineTo(16.0, 4.0);
				pencil.lineTo(20.0, 8.0);
				pencil.lineTo(9.0, 19.0);
				pencil.closeSubpath();
				aPainter.drawPath(pencil);
				aPainter.drawLine(QPointF{ 13.5, 6.5 }, QPointF{ 17.5, 10.5 });
				break;
			}
			case share_glyph::fixed: {
				aPainter.drawRoundedRect(QRectF{ 5.0, 11.0, 14.0, 10.0 }, 2.0, 2.0);
				auto shackle = QPainterPath{ };
				shackle.moveTo(8.0, 11.0);
				shackle.lineTo(8.0, 8.0);
				shackle.arcTo(QRectF{ 8.0, 4.0, 8.0, 8.0 }, 180.0, -180.0);
				shackle.lineTo(16.0, 11.0);
				aPainter.drawPath(shackle);
				break;
			}
			case share_glyph::information:
				ring();
				aPainter.drawLine(QPointF{ 12.0, 11.0 }, QPointF{ 12.0, 16.5 });
				dot(QPointF{ 12.0, 7.6 });
				break;
			case share_glyph::caution: {
				auto triangle = QPainterPath{ };
				triangle.moveTo(12.0, 3.5);
				triangle.lineTo(21.5, 20.0);
				triangle.lineTo(2.5, 20.0);
				triangle.closeSubpath();
				aPainter.drawPath(triangle);
				aPainter.drawLine(QPointF{ 12.0, 9.5 }, QPointF{ 12.0, 14.0 });
				dot(QPointF{ 12.0, 17.0 });
				break;
			}
			}
		}

		/// The colour laid over `aBase` at `aAlpha` of 255, opaque.
		auto washed(const QColor& aColour, const QColor& aBase, const int aAlpha) -> QColor
		{
			const auto mix = [aAlpha](const int aOver, const int aUnder) { return (aOver * aAlpha + aUnder * (255 - aAlpha)) / 255; };
			return QColor{ mix(aColour.red(), aBase.red()), mix(aColour.green(), aBase.green()), mix(aColour.blue(), aBase.blue()) };
		}

		constexpr auto chip_wash    = 40;
		constexpr auto chip_outline = 120;
		constexpr auto panel_wash   = 26;
		constexpr auto panel_edge   = 70;

		/// Strokes `aGlyph` `aSide` logical pixels square at the origin in `aInk`.
		void paint_glyph(QPainter& aPainter, const share_glyph aGlyph, const qreal aSide, const QColor& aInk)
		{
			aPainter.save();
			const auto unit = aSide / 24.0;
			aPainter.scale(unit, unit);
			auto pen = QPen{ aInk };
			// About one and a half device pixels at 16 logical, whatever the size.
			pen.setWidthF(std::max(1.8, 1.3 / unit));
			pen.setCapStyle(Qt::RoundCap);
			pen.setJoinStyle(Qt::RoundJoin);
			aPainter.setPen(pen);
			aPainter.setBrush(Qt::NoBrush);
			stroke_glyph(aPainter, aGlyph);
			aPainter.restore();
		}
	}

	auto share_tone_role(const share_tone aTone) noexcept -> core::appearance_role
	{
		switch (aTone) {
		case share_tone::waiting:     return core::appearance_role::state_stale;
		case share_tone::local:       return core::appearance_role::state_accepted;
		case share_tone::onward:      return core::appearance_role::state_verified;
		case share_tone::changed:     return core::appearance_role::state_fixed;
		case share_tone::warning:     return core::appearance_role::syntax_warning;
		case share_tone::refused:     return core::appearance_role::state_defect;
		case share_tone::information: return core::appearance_role::state_unread;
		}
		return core::appearance_role::state_stale;
	}

	auto share_tone_colour(const share_tone aTone, const code_role_colors& aColours, const QPalette& aPalette) -> QColor
	{
		const auto& colour = aColours.at(static_cast<std::size_t>(share_tone_role(aTone)));
		return colour.isValid() ? colour : aPalette.color(QPalette::PlaceholderText);
	}

	auto share_glyph_pixmap(const share_glyph aGlyph, const QColor& aInk, const int aLogical, const qreal aScale) -> QPixmap
	{
		const auto device = static_cast<int>(std::lround(aLogical * aScale));
		auto       pixmap = QPixmap{ device, device };
		pixmap.setDevicePixelRatio(aScale);
		pixmap.fill(Qt::transparent);
		auto painter = QPainter{ &pixmap };
		painter.setRenderHint(QPainter::Antialiasing, true);
		paint_glyph(painter, aGlyph, static_cast<qreal>(aLogical), aInk);
		return pixmap;
	}

	auto share_glyph_icon(const share_glyph aGlyph, const QColor& aInk, const QColor& aDisabled) -> QIcon
	{
		return painted_glyph_icon([aGlyph](QPainter& aPainter, const qreal aSide, const QColor& aGlyphInk) { paint_glyph(aPainter, aGlyph, aSide, aGlyphInk); }, aInk, aDisabled);
	}

	share_chip::share_chip(QWidget* const aParent)
		: QLabel{ aParent }
	{
		setTextFormat(Qt::PlainText);
		setFocusPolicy(Qt::NoFocus);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}

	void share_chip::set_mark(const share_glyph aGlyph, const QColor& aColour, const QString& aWords)
	{
		if (Glyph == aGlyph && Colour == aColour && text() == aWords) {
			return;
		}
		Glyph  = aGlyph;
		Colour = aColour;
		setText(aWords);
		updateGeometry();
		update();
	}

	auto share_chip::glyph() const noexcept -> share_glyph
	{
		return Glyph;
	}

	auto share_chip::colour() const noexcept -> const QColor&
	{
		return Colour;
	}

	auto share_chip::sizeHint() const -> QSize
	{
		const auto metrics = QFontMetrics{ font() };
		const auto height  = metrics.height() + 6;
		const auto glyph   = metrics.height();
		return QSize{ 8 + glyph + 5 + metrics.horizontalAdvance(text()) + 10, height };
	}

	auto share_chip::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void share_chip::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto base    = palette().color(QPalette::Window);
		const auto metrics = QFontMetrics{ font() };
		const auto box     = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		const auto radius  = box.height() / 2.0;
		painter.setPen(QPen{ washed(Colour, base, chip_outline), 1.0 });
		painter.setBrush(washed(Colour, base, chip_wash));
		painter.drawRoundedRect(box, radius, radius);
		const auto glyph = metrics.height();
		const auto at    = QPointF{ 8.0, (height() - glyph) / 2.0 };
		painter.drawPixmap(at, share_glyph_pixmap(Glyph, Colour, glyph, devicePixelRatioF()));
		painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
		painter.setFont(font());
		painter.drawText(QRectF{ 8.0 + glyph + 5.0, 0.0, width() - (8.0 + glyph + 5.0), static_cast<qreal>(height()) }, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
	}

	share_panel::share_panel(QWidget* const aParent)
		: QWidget{ aParent }
	{
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(10, 6, 10, 6);
		row->setSpacing(8);
		Mark = new QLabel{ this };
		Mark->setObjectName(QStringLiteral("cycleShareMark"));
		Mark->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
		row->addWidget(Mark, 0, Qt::AlignTop);
	}

	void share_panel::set_mark(const share_glyph aGlyph, const QColor& aColour)
	{
		if (Glyph == aGlyph && Colour == aColour && Mark->pixmap().cacheKey() != 0) {
			return;
		}
		Glyph  = aGlyph;
		Colour = aColour;
		repaint_mark();
		update();
	}

	auto share_panel::glyph() const noexcept -> share_glyph
	{
		return Glyph;
	}

	auto share_panel::colour() const noexcept -> const QColor&
	{
		return Colour;
	}

	auto share_panel::mark() const noexcept -> QLabel*
	{
		return Mark;
	}

	void share_panel::repaint_mark()
	{
		const auto size = QFontMetrics{ font() }.height();
		Mark->setPixmap(share_glyph_pixmap(Glyph, Colour, size, devicePixelRatioF()));
		Mark->setFixedSize(size, size);
	}

	void share_panel::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::DevicePixelRatioChange) {
			repaint_mark();
		}
	}

	void share_panel::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto base = palette().color(QPalette::Window);
		painter.setPen(QPen{ washed(Colour, base, panel_edge), 1.0 });
		painter.setBrush(washed(Colour, base, panel_wash));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
	}
}
