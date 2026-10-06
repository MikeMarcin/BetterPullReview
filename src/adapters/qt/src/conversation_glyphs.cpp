#include "conversation_glyphs.hpp"

#include "painted_glyph_icon.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>

#include <array>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr qreal glyph_grid = 24.0;
		constexpr qreal glyph_pen  = 1.9;

		void dot(QPainter& aPainter, const QPointF aCentre, const qreal aRadius)
		{
			aPainter.save();
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(aPainter.pen().color());
			aPainter.drawEllipse(aCentre, aRadius, aRadius);
			aPainter.restore();
		}

		void polyline(QPainter& aPainter, std::initializer_list<QPointF> aPoints)
		{
			auto path  = QPainterPath{ };
			auto first = true;
			for (const auto point : aPoints) {
				if (first) {
					path.moveTo(point);
					first = false;
				} else {
					path.lineTo(point);
				}
			}
			aPainter.drawPath(path);
		}

		void stroke(QPainter& aPainter, const conversation_glyph aGlyph)
		{
			switch (aGlyph) {
			case conversation_glyph::target:
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 6.5, 6.5);
				dot(aPainter, { 12.0, 12.0 }, 1.6);
				aPainter.drawLine(QPointF{ 12.0, 2.5 }, QPointF{ 12.0, 5.5 });
				aPainter.drawLine(QPointF{ 12.0, 18.5 }, QPointF{ 12.0, 21.5 });
				aPainter.drawLine(QPointF{ 2.5, 12.0 }, QPointF{ 5.5, 12.0 });
				aPainter.drawLine(QPointF{ 18.5, 12.0 }, QPointF{ 21.5, 12.0 });
				break;
			case conversation_glyph::resolve:
			case conversation_glyph::resolved:
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 8.5, 8.5);
				polyline(aPainter, { { 8.3, 12.3 }, { 10.8, 14.8 }, { 15.8, 9.4 } });
				break;
			case conversation_glyph::unresolve:
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 8.5, 8.5);
				aPainter.drawLine(QPointF{ 9.0, 9.0 }, QPointF{ 15.0, 15.0 });
				aPainter.drawLine(QPointF{ 15.0, 9.0 }, QPointF{ 9.0, 15.0 });
				break;
			case conversation_glyph::read:
				polyline(aPainter, { { 2.5, 12.5 }, { 6.1, 16.1 }, { 13.0, 9.0 } });
				polyline(aPainter, { { 10.6, 16.1 }, { 11.4, 16.9 }, { 20.5, 8.0 } });
				break;
			case conversation_glyph::more:
				dot(aPainter, { 5.5, 12.0 }, 1.5);
				dot(aPainter, { 12.0, 12.0 }, 1.5);
				dot(aPainter, { 18.5, 12.0 }, 1.5);
				break;
			case conversation_glyph::collapse:
			case conversation_glyph::up:
				polyline(aPainter, { { 6.0, 14.5 }, { 12.0, 8.5 }, { 18.0, 14.5 } });
				break;
			case conversation_glyph::down:
				polyline(aPainter, { { 6.0, 9.5 }, { 12.0, 15.5 }, { 18.0, 9.5 } });
				break;
			case conversation_glyph::reply: {
				polyline(aPainter, { { 9.5, 7.0 }, { 5.0, 11.5 }, { 9.5, 16.0 } });
				auto tail = QPainterPath{ };
				tail.moveTo(5.0, 11.5);
				tail.lineTo(14.0, 11.5);
				tail.arcTo(QRectF{ 9.0, 11.5, 10.0, 10.0 }, 90.0, -90.0);
				tail.lineTo(19.0, 18.0);
				aPainter.drawPath(tail);
				break;
			}
			case conversation_glyph::send: {
				auto plane = QPainterPath{ };
				plane.moveTo(4.0, 11.8);
				plane.lineTo(20.0, 4.5);
				plane.lineTo(13.8, 20.0);
				plane.lineTo(11.1, 13.5);
				plane.closeSubpath();
				aPainter.drawPath(plane);
				aPainter.drawLine(QPointF{ 11.1, 13.5 }, QPointF{ 20.0, 4.5 });
				break;
			}
			case conversation_glyph::lock: {
				aPainter.drawRoundedRect(QRectF{ 5.5, 11.0, 13.0, 9.0 }, 2.0, 2.0);
				auto shackle = QPainterPath{ };
				shackle.moveTo(8.5, 11.0);
				shackle.lineTo(8.5, 8.0);
				shackle.arcTo(QRectF{ 8.5, 4.5, 7.0, 7.0 }, 180.0, -180.0);
				shackle.lineTo(15.5, 11.0);
				aPainter.drawPath(shackle);
				break;
			}
			case conversation_glyph::edit: {
				auto pencil = QPainterPath{ };
				pencil.moveTo(4.0, 20.0);
				pencil.lineTo(8.0, 20.0);
				pencil.lineTo(19.0, 9.0);
				pencil.lineTo(15.0, 5.0);
				pencil.lineTo(4.0, 16.0);
				pencil.closeSubpath();
				aPainter.drawPath(pencil);
				aPainter.drawLine(QPointF{ 13.5, 6.5 }, QPointF{ 17.5, 10.5 });
				break;
			}
			case conversation_glyph::trash:
				aPainter.drawLine(QPointF{ 5.0, 7.0 }, QPointF{ 19.0, 7.0 });
				polyline(aPainter, { { 10.0, 7.0 }, { 10.0, 4.5 }, { 14.0, 4.5 }, { 14.0, 7.0 } });
				polyline(aPainter, { { 7.0, 7.0 }, { 8.0, 20.0 }, { 16.0, 20.0 }, { 17.0, 7.0 } });
				break;
			case conversation_glyph::only_new:
				dot(aPainter, { 12.0, 12.0 }, 4.0);
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 8.5, 8.5);
				break;
			case conversation_glyph::hide_resolved:
				aPainter.drawEllipse(QPointF{ 12.0, 12.0 }, 8.5, 8.5);
				aPainter.drawLine(QPointF{ 8.0, 16.0 }, QPointF{ 16.0, 8.0 });
				break;
			case conversation_glyph::next_new:
				dot(aPainter, { 12.0, 7.0 }, 3.0);
				aPainter.drawLine(QPointF{ 12.0, 12.5 }, QPointF{ 12.0, 20.5 });
				polyline(aPainter, { { 8.5, 17.0 }, { 12.0, 20.5 }, { 15.5, 17.0 } });
				break;
			}
		}

		[[nodiscard]] auto disabled_ink(const QPalette& aPalette) -> QColor
		{
			auto ink = aPalette.color(QPalette::Disabled, QPalette::ButtonText);
			if (ink == aPalette.color(QPalette::Active, QPalette::ButtonText)) {
				ink.setAlphaF(0.4F);
			}
			return ink;
		}

		[[nodiscard]] auto icon_of(const conversation_glyph aGlyph, const QColor& aInk, const QColor& aDisabled) -> QIcon
		{
			// [[BPR-1296]]. Painted at the size and scale the button asks for.
			return painted_glyph_icon([aGlyph](QPainter& aPainter, const qreal aSide, const QColor& aGlyphInk) { paint_conversation_glyph(aPainter, QRectF{ 0.0, 0.0, aSide, aSide }, aGlyph, aGlyphInk); },
									  aInk, aDisabled);
		}
	} // namespace

	void paint_conversation_glyph(QPainter& aPainter, const QRectF& aBox, const conversation_glyph aGlyph, const QColor& aInk)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.translate(aBox.topLeft());
		const auto unit = std::min(aBox.width(), aBox.height()) / glyph_grid;
		aPainter.scale(unit, unit);
		auto pen = QPen{ aInk };
		pen.setWidthF(glyph_pen);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		aPainter.setPen(pen);
		aPainter.setBrush(Qt::NoBrush);
		stroke(aPainter, aGlyph);
		aPainter.restore();
	}

	auto conversation_icon(const conversation_glyph aGlyph, const QPalette& aPalette) -> QIcon
	{
		return icon_of(aGlyph, aPalette.color(QPalette::Active, QPalette::ButtonText), disabled_ink(aPalette));
	}

	auto conversation_icon(const conversation_glyph aGlyph, const QColor& aInk) -> QIcon
	{
		auto disabled = aInk;
		disabled.setAlphaF(0.4F);
		return icon_of(aGlyph, aInk, disabled);
	}
} // namespace bpr::qt
