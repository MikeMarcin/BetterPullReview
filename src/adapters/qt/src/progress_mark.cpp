#include "bpr/qt/progress_mark.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include "painted_glyph_icon.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QPointF>
#include <QRectF>

#include <algorithm>
#include <numbers>

namespace bpr::qt {
	namespace {
		/// The design's box, circle and stroke, in units of a 24-unit box: a
		/// ring is stroked on radius 7 at width 2, so every mark, ring or
		/// disc, is 16 units across.
		constexpr qreal design_box   = 24.0;
		constexpr qreal ring_radius  = 7.0;
		constexpr qreal disc_radius  = 8.0;
		constexpr qreal stroke_width = 2.0;
		/// The tick cut out of the accepted disc, as the v002 button drew it.
		constexpr qreal tick_width = 2.2;
		/// The unknown ring's dashes, in stroke widths: eight round the ring.
		constexpr qreal dash_drawn = 1.6;
		constexpr qreal dash_open  = 1.15;
		/// The halo a menu's highlighted row draws round the mark.
		constexpr qreal halo_width = 1.5;

		[[nodiscard]] auto ring_pen(const QColor& aColour) -> QPen
		{
			auto pen = QPen{ aColour };
			pen.setWidthF(stroke_width);
			pen.setCapStyle(Qt::RoundCap);
			pen.setJoinStyle(Qt::RoundJoin);
			return pen;
		}

		/// Paints the mark in the 24-unit design box at the painter's origin.
		void paint_in_design_box(QPainter& aPainter, const std::optional<core::file_progress_mark> aMark, const progress_mark_colours& aColours)
		{
			const auto centre = QPointF{ design_box / 2.0, design_box / 2.0 };
			const auto ring   = QRectF{ centre.x() - ring_radius, centre.y() - ring_radius, 2.0 * ring_radius, 2.0 * ring_radius };
			aPainter.setBrush(Qt::NoBrush);
			if (!aMark.has_value()) {
				auto pen = ring_pen(aColours.Neutral);
				pen.setCapStyle(Qt::FlatCap);
				pen.setDashPattern({ dash_drawn, dash_open });
				aPainter.setPen(pen);
				aPainter.drawEllipse(ring);
				return;
			}
			switch (*aMark) {
			case core::file_progress_mark::unread:
				aPainter.setPen(ring_pen(aColours.Neutral));
				aPainter.drawEllipse(ring);
				return;
			case core::file_progress_mark::seen:
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(aColours.Seen);
				aPainter.drawPie(ring, 90 * 16, 180 * 16);
				aPainter.setBrush(Qt::NoBrush);
				aPainter.setPen(ring_pen(aColours.Seen));
				aPainter.drawEllipse(ring);
				return;
			case core::file_progress_mark::read:
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(aColours.Read);
				aPainter.drawEllipse(centre, disc_radius, disc_radius);
				return;
			case core::file_progress_mark::accepted: {
				auto disc = QPainterPath{ };
				disc.addEllipse(centre, disc_radius, disc_radius);
				auto tick = QPainterPath{ };
				tick.moveTo(8.0, 12.2);
				tick.lineTo(11.0, 15.2);
				tick.lineTo(16.2, 9.0);
				auto cut = QPainterPathStroker{ };
				cut.setWidth(tick_width);
				cut.setCapStyle(Qt::RoundCap);
				cut.setJoinStyle(Qt::RoundJoin);
				aPainter.fillPath(disc.subtracted(cut.createStroke(tick)), aColours.Accepted);
				return;
			}
			case core::file_progress_mark::excluded: {
				aPainter.setPen(ring_pen(aColours.Neutral));
				aPainter.drawEllipse(ring);
				const auto reach = ring_radius * std::numbers::sqrt2 / 2.0;
				aPainter.setPen(ring_pen(aColours.Excluded));
				aPainter.drawLine(QPointF{ centre.x() - reach, centre.y() + reach }, QPointF{ centre.x() + reach, centre.y() - reach });
				return;
			}
			}
		}

		[[nodiscard]] auto colours_of(const core::appearance_palette& aPalette) -> progress_mark_colours
		{
			const auto colour = [&aPalette](const core::appearance_role aRole) {
				return to_presentation_color(aPalette[core::index_of(aRole)]);
			};
			return progress_mark_colours{
				.Neutral  = colour(core::appearance_role::state_stale),
				.Seen     = colour(core::appearance_role::state_attention),
				.Read     = colour(core::appearance_role::state_unread),
				.Accepted = colour(core::appearance_role::state_accepted),
				.Excluded = colour(core::appearance_role::state_defect),
			};
		}
	} // namespace

	auto progress_mark_colours_for(const appearance_profile& aProfile) -> progress_mark_colours
	{
		return progress_mark_colours{
			.Neutral  = aProfile.color(core::appearance_role::state_stale),
			.Seen     = aProfile.color(core::appearance_role::state_attention),
			.Read     = aProfile.color(core::appearance_role::state_unread),
			.Accepted = aProfile.color(core::appearance_role::state_accepted),
			.Excluded = aProfile.color(core::appearance_role::state_defect),
		};
	}

	auto builtin_progress_mark_colours(const core::appearance_variant aVariant) -> progress_mark_colours
	{
		return colours_of(builtin_appearance_palette(aVariant));
	}

	auto monochrome_progress_mark_colours(const QColor& aInk) -> progress_mark_colours
	{
		return progress_mark_colours{ .Neutral = aInk, .Seen = aInk, .Read = aInk, .Accepted = aInk, .Excluded = aInk };
	}

	void paint_progress_mark(QPainter& aPainter, const QRectF& aBox, const std::optional<core::file_progress_mark> aMark, const progress_mark_colours& aColours)
	{
		const auto side = std::min(aBox.width(), aBox.height());
		if (side <= 0.0) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.translate(aBox.center() - QPointF{ side / 2.0, side / 2.0 });
		const auto unit = side / design_box;
		aPainter.scale(unit, unit);
		paint_in_design_box(aPainter, aMark, aColours);
		aPainter.restore();
	}

	auto progress_mark_icon(
		const std::optional<core::file_progress_mark> aMark,
		const progress_mark_colours&                  aColours,
		const QColor&                                 aDisabled,
		const std::optional<QColor>                   aActiveHalo) -> QIcon
	{
		return painted_mode_icon([aMark, aColours, aDisabled, aActiveHalo](QPainter& aPainter, const qreal aSide, const QIcon::Mode aMode) {
			const auto box = QRectF{ 0.0, 0.0, aSide, aSide };
			if (aMode == QIcon::Disabled) {
				paint_progress_mark(aPainter, box, aMark, monochrome_progress_mark_colours(aDisabled));
				return;
			}
			if (aMode == QIcon::Active && aActiveHalo.has_value()) {
				// A ring in the menu's own background just outside the mark.
				const auto unit   = aSide / design_box;
				const auto radius = (disc_radius + (halo_width / 2.0)) * unit;
				aPainter.save();
				aPainter.setPen(QPen{ *aActiveHalo, halo_width * unit });
				aPainter.setBrush(Qt::NoBrush);
				aPainter.drawEllipse(box.center(), radius, radius);
				aPainter.restore();
			}
			paint_progress_mark(aPainter, box, aMark, aColours);
		});
	}
} // namespace bpr::qt
