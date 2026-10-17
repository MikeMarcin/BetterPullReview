#include "bpr/qt/disclosure_button.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"

#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		/// \brief The bar's geometry: an inset, the chevron, a gap, the title
		/// and the inset again, as the review bar's Updates disclosure draws it.
		constexpr int   inset          = 6;
		constexpr int   chevron        = 7;
		constexpr int   gap            = 5;
		constexpr int   least_height   = 22;
		constexpr int   text_margin    = 6;
		constexpr qreal corner         = 4.0;
		constexpr int   hover_alpha    = 60;
		constexpr int   pressed_alpha  = 110;
		constexpr qreal readable_ratio = 4.5;

		[[nodiscard]] auto ratio(const QColor& aOne, const QColor& aOther) -> double
		{
			return contrast_ratio(to_appearance_color(aOne), to_appearance_color(aOther));
		}
	} // namespace

	auto disclosure_fill(const QPalette& aPalette, const QPalette::ColorGroup aGroup) -> QColor
	{
		const auto raised = aPalette.color(aGroup, QPalette::Light);
		const auto text   = aPalette.color(aGroup, QPalette::WindowText);
		return ratio(raised, text) >= readable_ratio ? raised : aPalette.color(aGroup, QPalette::Window);
	}

	disclosure_button::disclosure_button(QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setCheckable(true);
		setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		setArrowType(Qt::RightArrow);
		setAttribute(Qt::WA_Hover, true);
		// The arrow is the state: a host that opens the section by `setChecked`
		// gets the down arrow without saying so.
		connect(this, &QToolButton::toggled, this, [this](const bool aOpen) { setArrowType(aOpen ? Qt::DownArrow : Qt::RightArrow); });
	}

	auto disclosure_button::sizeHint() const -> QSize
	{
		// The title's fractional advance, rounded up: an integer advance can
		// round down and leave the whole title a pixel short, so it elides.
		const auto title = static_cast<int>(std::ceil(QFontMetricsF{ font() }.horizontalAdvance(text()))) + 1;
		return QSize{ inset + chevron + gap + title + inset, std::max(least_height, fontMetrics().height() + text_margin) };
	}

	auto disclosure_button::minimumSizeHint() const -> QSize
	{
		// A narrow column shortens the title with an ellipsis; the chevron stays.
		return QSize{ inset + chevron + gap + inset, sizeHint().height() };
	}

	void disclosure_button::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  group   = isEnabled() ? QPalette::Active : QPalette::Disabled;
		const auto  frame   = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		const auto  fill    = disclosure_fill(colours, group);
		painter.setPen(Qt::NoPen);
		painter.setBrush(fill);
		painter.drawRoundedRect(frame, corner, corner);
		if (isEnabled() && (isDown() || underMouse())) {
			auto tint = colours.color(QPalette::Active, QPalette::Mid);
			tint.setAlpha(isDown() ? pressed_alpha : hover_alpha);
			painter.setBrush(tint);
			painter.drawRoundedRect(frame, corner, corner);
		}

		const auto ink    = colours.color(group, QPalette::WindowText);
		const auto middle = (static_cast<qreal>(height()) / 2.0);
		const auto left   = static_cast<qreal>(inset);
		const auto side   = static_cast<qreal>(chevron);
		auto       arrow  = QPainterPath{ };
		if (isChecked()) {
			arrow.moveTo(left, middle - (side * 0.3));
			arrow.lineTo(left + side, middle - (side * 0.3));
			arrow.lineTo(left + (side / 2.0), middle + (side * 0.45));
		} else {
			arrow.moveTo(left + (side * 0.15), middle - (side / 2.0));
			arrow.lineTo(left + (side * 0.9), middle);
			arrow.lineTo(left + (side * 0.15), middle + (side / 2.0));
		}
		arrow.closeSubpath();
		painter.setBrush(ink);
		painter.drawPath(arrow);

		const auto textLeft = inset + chevron + gap;
		const auto room     = std::max(0, width() - textLeft - inset);
		painter.setPen(ink);
		painter.drawText(QRect{ textLeft, 0, room, height() }, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, room));

		if (hasFocus()) {
			// [[BPR-1157]]. The window's focus ring: a style's focus rectangle
			// is nothing at all on macOS.
			constexpr auto ring = focus_ring_width / 2.0;
			painter.setPen(QPen{ focus_ring_colour(colours, fill == colours.color(group, QPalette::Light) ? QPalette::Light : QPalette::Window), focus_ring_width });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ rect() }.adjusted(ring, ring, -ring, -ring), corner - ring, corner - ring);
		}
	}
} // namespace bpr::qt
