#include "bpr/qt/identity_avatar.hpp"

#include "bpr/core/identity_avatar.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QPainter>
#include <QPen>

#include <algorithm>
#include <array>
#include <cmath>

namespace bpr::qt {
	namespace {
		constexpr auto one_letter_scale = 0.53;
		constexpr auto two_letter_scale = 0.44;
		constexpr auto least_pixels     = 6;
		constexpr auto group_corner     = 0.22;
		constexpr auto ring_offset      = 2.0;
	} // namespace

	auto identity_avatar_grounds_of(const QPalette& aPalette) -> identity_avatar_grounds
	{
		return { .Window = aPalette.color(QPalette::Active, QPalette::Window), .Base = aPalette.color(QPalette::Active, QPalette::Base) };
	}

	auto identity_avatar_colours_for(const QString& aIdentity, const identity_avatar_grounds& aGrounds) -> identity_avatar_colours
	{
		const auto grounds = std::array{ to_appearance_color(aGrounds.Window), to_appearance_color(aGrounds.Base) };
		const auto tone    = core::identity_tone_for(aIdentity.toStdString(), grounds);
		return { .Fill = to_presentation_color(tone.Fill), .Ink = to_presentation_color(tone.Ink) };
	}

	auto identity_avatar_initials(const QString& aName) -> QString
	{
		return QString::fromStdString(core::identity_initials(aName.toStdString()));
	}

	void paint_identity_avatar(
		QPainter&                      aPainter,
		const QRectF&                  aBox,
		const identity_avatar_face&    aFace,
		const identity_avatar_grounds& aGrounds,
		const QFont&                   aFont)
	{
		if (aBox.isEmpty()) {
			return;
		}
		const auto colours  = identity_avatar_colours_for(aFace.Identity, aGrounds);
		const auto initials = identity_avatar_initials(aFace.InitialsFrom.isEmpty() ? aFace.Identity : aFace.InitialsFrom);
		const auto group    = aFace.Shape == identity_avatar_shape::group;
		const auto corner   = aBox.height() * group_corner;
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		if (aFace.Ring.isValid()) {
			aPainter.setPen(QPen{ aFace.Ring, 1.0 });
			aPainter.setBrush(Qt::NoBrush);
			const auto ring = aBox.adjusted(-ring_offset, -ring_offset, ring_offset, ring_offset);
			if (group) {
				aPainter.drawRoundedRect(ring, corner + ring_offset, corner + ring_offset);
			} else {
				aPainter.drawEllipse(ring);
			}
		}
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(colours.Fill);
		if (group) {
			aPainter.drawRoundedRect(aBox, corner, corner);
		} else {
			aPainter.drawEllipse(aBox);
		}
		auto font = aFont;
		font.setWeight(QFont::Bold);
		const auto scale = initials.size() > 1 && !initials.front().isHighSurrogate() ? two_letter_scale : one_letter_scale;
		font.setPixelSize(std::max(least_pixels, static_cast<int>(std::lround(aBox.height() * scale))));
		aPainter.setFont(font);
		aPainter.setPen(colours.Ink);
		aPainter.drawText(aBox, Qt::AlignCenter, initials);
		aPainter.restore();
	}

	identity_avatar_view::identity_avatar_view(const int aSide, QWidget* const aParent)
		: QWidget{ aParent }
		, Side{ aSide }
	{
		setFixedSize(aSide, aSide);
		setAttribute(Qt::WA_TranslucentBackground, true);
	}

	void identity_avatar_view::set_identity(const QString& aIdentity)
	{
		if (Identity == aIdentity) {
			return;
		}
		Identity = aIdentity;
		update();
	}

	auto identity_avatar_view::identity() const -> const QString&
	{
		return Identity;
	}

	auto identity_avatar_view::sizeHint() const -> QSize
	{
		return { Side, Side };
	}

	void identity_avatar_view::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		paint_identity_avatar(painter, QRectF{ rect() }, { .Identity = Identity }, identity_avatar_grounds_of(palette()), font());
	}
} // namespace bpr::qt
