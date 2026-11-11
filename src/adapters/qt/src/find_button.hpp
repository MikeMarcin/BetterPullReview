#ifndef BPR_SRC_ADAPTERS_QT_SRC_FIND_BUTTON_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_FIND_BUTTON_HPP

#include "bpr/qt/keyboard_focus_ring.hpp"

#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPainter>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bpr::qt {
	enum class find_glyph : std::uint8_t { match_case,
										   whole_word,
										   regular_expression,
										   selection,
										   previous,
										   next,
										   close };

	/// \brief The side of every square control, from the UI font.
	[[nodiscard]] inline auto find_control_side(const QFontMetrics& aMetrics) -> int
	{
		return std::max(20, static_cast<int>(std::lround(aMetrics.height() * 1.3)));
	}

	/// \brief One square find control, drawn rather than labelled.
	///
	/// \details VS Code's find widget is icons, not words; text buttons
	/// ("Word", "Selection") were what made the old bar read as a form.
	/// Each glyph is painted from the palette so it follows the theme and
	/// stays sharp at every scale. A checked toggle is an accent tint with
	/// an accent outline, the one mark that says "this option is on". The
	/// outline is the accent lifted to the indicator floor on the ground the
	/// button sits on (`focus_ring_colour`): a theme whose accent is a pale
	/// selection colour left "on" at about 2:1 against "off".
	class find_button final : public QToolButton
	{
	public:
		find_button(QWidget* const aParent, const find_glyph aGlyph)
			: QToolButton{ aParent }
			, Glyph{ aGlyph }
		{
			setAutoRaise(true);
			setFocusPolicy(Qt::TabFocus);
			setCursor(Qt::ArrowCursor);
		}
		[[nodiscard]] auto sizeHint() const -> QSize override
		{
			const auto side = find_control_side(fontMetrics());
			return { side, side };
		}
		[[nodiscard]] auto minimumSizeHint() const -> QSize override { return sizeHint(); }

	protected:
		void paintEvent(QPaintEvent* const aEvent) override
		{
			static_cast<void>(aEvent);
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto box     = QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5);
			const auto accent  = palette().color(QPalette::Active, QPalette::Highlight);
			const auto ground  = parentWidget() == nullptr ? QPalette::Window : parentWidget()->backgroundRole();
			const auto outline = focus_ring_colour(palette(), ground);
			if (isChecked()) {
				auto tint = accent;
				tint.setAlpha(isDown() ? 96 : 56);
				painter.setPen(QPen{ outline, 1.0 });
				painter.setBrush(tint);
				painter.drawRoundedRect(box, 3.0, 3.0);
			} else if (isEnabled() && (underMouse() || isDown())) {
				auto hover = palette().color(QPalette::Active, QPalette::Mid);
				hover.setAlpha(isDown() ? 110 : 64);
				painter.setPen(Qt::NoPen);
				painter.setBrush(hover);
				painter.drawRoundedRect(box, 3.0, 3.0);
			}
			if (hasFocus()) {
				painter.setPen(QPen{ outline, 1.5 });
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(box, 3.0, 3.0);
			}
			const auto ink = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText);
			painter.setPen(QPen{ ink, std::max(1.2, width() / 14.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			painter.setBrush(Qt::NoBrush);
			const auto c    = QRectF{ rect() }.center();
			const auto unit = std::min(width(), height()) / 20.0;
			switch (Glyph) {
			case find_glyph::match_case:
			case find_glyph::whole_word:
			case find_glyph::regular_expression: paint_text_glyph(painter, ink); break;
			case find_glyph::selection:
				// Lines of text inside a selection bracket.
				painter.drawLine(QPointF{ c.x() - 5.0 * unit, c.y() - 4.0 * unit }, QPointF{ c.x() + 5.0 * unit, c.y() - 4.0 * unit });
				painter.drawLine(QPointF{ c.x() - 5.0 * unit, c.y() }, QPointF{ c.x() + 2.0 * unit, c.y() });
				painter.drawLine(QPointF{ c.x() - 5.0 * unit, c.y() + 4.0 * unit }, QPointF{ c.x() + 4.0 * unit, c.y() + 4.0 * unit });
				break;
			case find_glyph::previous:
			case find_glyph::next:     {
				const auto sign = Glyph == find_glyph::previous ? -1.0 : 1.0;
				const auto tip  = QPointF{ c.x(), c.y() + sign * 5.5 * unit };
				painter.drawLine(QPointF{ c.x(), c.y() - sign * 5.5 * unit }, tip);
				painter.drawLine(tip, QPointF{ c.x() - 4.0 * unit, tip.y() - sign * 4.0 * unit });
				painter.drawLine(tip, QPointF{ c.x() + 4.0 * unit, tip.y() - sign * 4.0 * unit });
				break;
			}
			case find_glyph::close:
				painter.drawLine(QPointF{ c.x() - 4.5 * unit, c.y() - 4.5 * unit }, QPointF{ c.x() + 4.5 * unit, c.y() + 4.5 * unit });
				painter.drawLine(QPointF{ c.x() + 4.5 * unit, c.y() - 4.5 * unit }, QPointF{ c.x() - 4.5 * unit, c.y() + 4.5 * unit });
				break;
			}
		}

	private:
		find_glyph Glyph;

		void paint_text_glyph(QPainter& aPainter, const QColor& aInk) const
		{
			auto glyphFont = font();
			glyphFont.setPointSizeF(std::max(7.0, glyphFont.pointSizeF() * 0.92));
			glyphFont.setWeight(Glyph == find_glyph::regular_expression ? QFont::Bold : QFont::DemiBold);
			aPainter.setFont(glyphFont);
			aPainter.setPen(aInk);
			const auto text    = Glyph == find_glyph::match_case ? QStringLiteral("Aa") : Glyph == find_glyph::whole_word ? QStringLiteral("ab")
																														  : QStringLiteral(".*");
			const auto metrics = QFontMetricsF{ glyphFont };
			const auto advance = metrics.horizontalAdvance(text);
			const auto left    = (width() - advance) / 2.0;
			const auto base    = (height() + metrics.capHeight()) / 2.0 - (Glyph == find_glyph::whole_word ? 1.0 : 0.0);
			aPainter.drawText(QPointF{ left, base }, text);
			if (Glyph == find_glyph::whole_word) {
				// The underline is what says "word": a bounded stretch of text.
				aPainter.setPen(QPen{ aInk, 1.2, Qt::SolidLine, Qt::FlatCap });
				const auto y = base + 2.5;
				aPainter.drawLine(QPointF{ left - 1.0, y }, QPointF{ left + advance + 1.0, y });
			}
		}
	};

}

#endif
