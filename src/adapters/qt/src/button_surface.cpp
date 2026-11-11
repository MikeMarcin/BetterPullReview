#include "button_surface.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"

#include <QAbstractButton>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QRect>
#include <QStyle>
#include <QStyleOption>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		constexpr qreal chip_radius = 3.0;

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			const auto channel = [aAmount](const float aA, const float aB) {
				return static_cast<float>(aA + ((aB - aA) * aAmount));
			};
			return QColor::fromRgbF(channel(aFrom.redF(), aTo.redF()), channel(aFrom.greenF(), aTo.greenF()), channel(aFrom.blueF(), aTo.blueF()));
		}

		[[nodiscard]] auto chip_border(const QColor& aGround, const QColor& aInk) -> QColor
		{
			const auto ground = to_appearance_color(aGround);
			for (auto step = 10; step < 50; ++step) {
				const auto border = blend(aGround, aInk, step / 50.0);
				if (contrast_ratio(to_appearance_color(border), ground) >= 3.0) {
					return border;
				}
			}
			return aInk;
		}

		/// \brief A face `aAmount` of the way from `aGround` toward `aInk`, or
		/// less: never so far that the ink reads worse on it than it does on
		/// the ground, up to the high-contrast text floor (7:1). A link's
		/// words at 7.3:1 stay at 7:1 or more when pressed.
		[[nodiscard]] auto face(const QColor& aGround, const QColor& aInk, const qreal aAmount) -> QColor
		{
			const auto ink   = to_appearance_color(aInk);
			const auto limit = std::min(contrast_ratio(ink, to_appearance_color(aGround)), 7.0);
			// In hundredths of the way, from the asked amount down.
			for (auto hundredths = static_cast<int>(std::lround(aAmount * 100.0)); hundredths > 0; --hundredths) {
				const auto shade = blend(aGround, aInk, hundredths / 100.0);
				if (contrast_ratio(ink, to_appearance_color(shade)) + 0.005 >= limit) {
					return shade;
				}
			}
			return aGround;
		}

		[[nodiscard]] auto words_size(const QAbstractButton& aButton) -> QSize
		{
			if (aButton.text().isEmpty()) {
				return { };
			}
			const auto words = QFontMetricsF{ aButton.font() }.size(Qt::TextShowMnemonic, aButton.text());
			return { static_cast<int>(std::ceil(words.width())), static_cast<int>(std::ceil(words.height())) };
		}
	} // namespace

	void update_button_keyboard_focus(bool& aKeyboardFocus, const QFocusEvent& aEvent)
	{
		const auto reason = aEvent.reason();
		if (reason == Qt::MouseFocusReason) {
			aKeyboardFocus = false;
		} else if (reason != Qt::ActiveWindowFocusReason && reason != Qt::PopupFocusReason) {
			aKeyboardFocus = true;
		}
	}

	auto button_ink_role(const QPalette::ColorRole aSurface) -> QPalette::ColorRole
	{
		return aSurface == QPalette::Base ? QPalette::Text : QPalette::WindowText;
	}

	auto button_colours_on(const QPalette& aPalette, const QPalette::ColorRole aSurface) -> button_colours
	{
		const auto ink = button_ink_role(aSurface);
		return {
			.Ground      = aPalette.color(QPalette::Active, aSurface),
			.Ink         = aPalette.color(QPalette::Active, ink),
			.DisabledInk = aPalette.color(QPalette::Disabled, ink),
			.Focus       = focus_ring_colour(aPalette, aSurface),
		};
	}

	auto button_colours_on(const QPalette& aPalette, const QColor& aGround, const QColor& aInk) -> button_colours
	{
		auto ground = aPalette;
		ground.setColor(QPalette::Active, QPalette::Window, aGround);
		return {
			.Ground      = aGround,
			.Ink         = aInk,
			.DisabledInk = blend(aInk, aGround, 0.5),
			.Focus       = focus_ring_colour(ground, QPalette::Window),
		};
	}

	void paint_button_surface(QPainter& aPainter, const QAbstractButton& aButton, const bool aRaised, const bool aKeyboardFocus, const QPalette::ColorRole aSurface)
	{
		paint_button_surface(aPainter, aButton, button_colours_on(aButton.palette(), aSurface), aRaised, aKeyboardFocus);
	}

	void paint_button_surface(QPainter& aPainter, const QAbstractButton& aButton, const button_colours& aColours, const bool aRaised, const bool aKeyboardFocus)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto& ground  = aColours.Ground;
		const auto& ink     = aButton.isEnabled() ? aColours.Ink : aColours.DisabledInk;
		const auto  box     = QRectF{ aButton.rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		const auto  pressed = aButton.isEnabled() && aButton.isDown();
		if (aRaised) {
			aPainter.setPen(QPen{ chip_border(ground, ink), 1.0 });
			aPainter.setBrush(face(ground, ink, pressed ? 0.12 : 0.07));
			aPainter.drawRoundedRect(box, chip_radius, chip_radius);
		} else if (aButton.isEnabled() && (pressed || aButton.underMouse())) {
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(face(ground, ink, pressed ? 0.12 : 0.05));
			aPainter.drawRoundedRect(box, chip_radius, chip_radius);
		}
		if (aButton.isEnabled() && aButton.hasFocus() && aKeyboardFocus) {
			const auto inset = focus_ring_width / 2.0;
			aPainter.setPen(QPen{ aColours.Focus, focus_ring_width });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawRoundedRect(box.adjusted(inset, inset, -inset, -inset), chip_radius, chip_radius);
		}
		aPainter.restore();
	}

	auto chip_content_size(const QAbstractButton& aButton, const QSize aIconSize, const bool aShowIcon) -> QSize
	{
		const auto words = words_size(aButton);
		if (!aShowIcon) {
			return words;
		}
		return { aIconSize.width() + chip_icon_gap + words.width(), std::max(aIconSize.height(), words.height()) };
	}

	void paint_button_label(QPainter& aPainter, const QAbstractButton& aButton, const QSize aIconSize, const bool aShowIcon, const QPalette::ColorRole aSurface)
	{
		paint_button_label(aPainter, aButton, button_colours_on(aButton.palette(), aSurface), aIconSize, aShowIcon ? button_mark::icon : button_mark::none);
	}

	void paint_button_label(QPainter& aPainter, const QAbstractButton& aButton, const button_colours& aColours, const QSize aIconSize, const button_mark aMark,
							const Qt::ArrowType aArrow, const int aTrailing)
	{
		aPainter.save();
		auto* const style   = aButton.style();
		const auto  content = aButton.rect().adjusted(chip_padding, 0, -chip_padding - aTrailing, 0);
		const auto  shown   = aMark != button_mark::none;
		const auto  extent  = chip_content_size(aButton, aIconSize, shown);
		const auto& ink     = aButton.isEnabled() ? aColours.Ink : aColours.DisabledInk;
		// Centred in the padding; never starting before it when the button was
		// given less than it asked for.
		const auto left  = content.left() + std::max(0, (content.width() - extent.width()) / 2);
		auto       place = QRect{ left, content.top(), extent.width(), content.height() };
		if (shown) {
			const auto spot   = QRect{ place.left(), place.top() + ((place.height() - aIconSize.height()) / 2), aIconSize.width(), aIconSize.height() };
			const auto mirror = QStyle::visualRect(aButton.layoutDirection(), aButton.rect(), spot);
			if (aMark == button_mark::arrow) {
				paint_button_arrow(aPainter, QRectF{ mirror }, aArrow, ink);
			} else {
				const auto mode  = !aButton.isEnabled() ? QIcon::Disabled : (aButton.hasFocus() ? QIcon::Active : QIcon::Normal);
				const auto state = aButton.isChecked() ? QIcon::On : QIcon::Off;
				auto       image = aButton.icon().pixmap(aIconSize, aPainter.device()->devicePixelRatio(), mode, state);
				if (aMark == button_mark::tinted_icon) {
					image = tinted(image, ink);
				}
				style->drawItemPixmap(&aPainter, mirror, Qt::AlignCenter, image);
			}
			place.setLeft(place.left() + aIconSize.width() + chip_icon_gap);
		}
		auto option = QStyleOption{ };
		option.initFrom(&aButton);
		auto flags = static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextSingleLine);
		if (style->styleHint(QStyle::SH_UnderlineShortcut, &option, &aButton) == 0) {
			flags |= Qt::TextHideMnemonic;
		}
		auto words = aButton.palette();
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			words.setColor(group, QPalette::WindowText, ink);
		}
		style->drawItemText(&aPainter, QStyle::visualRect(aButton.layoutDirection(), aButton.rect(), place), flags, words, true, aButton.text(), QPalette::WindowText);
		aPainter.restore();
	}

	void paint_button_glyph(QPainter& aPainter, const QAbstractButton& aButton, const QSize aIconSize)
	{
		const auto mode  = aButton.isEnabled() ? QIcon::Normal : QIcon::Disabled;
		const auto state = aButton.isChecked() ? QIcon::On : QIcon::Off;
		const auto image = aButton.icon().pixmap(aIconSize, aPainter.device()->devicePixelRatio(), mode, state);
		aButton.style()->drawItemPixmap(&aPainter, aButton.rect(), Qt::AlignCenter, image);
	}

	void paint_button_menu_indicator(QPainter& aPainter, const QRectF& aPlace, const QColor& aInk)
	{
		paint_button_arrow(aPainter, aPlace, Qt::DownArrow, aInk);
	}

	void paint_button_glyph(QPainter& aPainter, const QAbstractButton& aButton, const QSize aIconSize, const QColor& aTint)
	{
		const auto state = aButton.isChecked() ? QIcon::On : QIcon::Off;
		const auto image = aButton.icon().pixmap(aIconSize, aPainter.device()->devicePixelRatio(), QIcon::Normal, state);
		aButton.style()->drawItemPixmap(&aPainter, aButton.rect(), Qt::AlignCenter, tinted(image, aTint));
	}

	auto tinted(const QPixmap& aImage, const QColor& aInk) -> QPixmap
	{
		if (aImage.isNull()) {
			return aImage;
		}
		auto result = QPixmap{ aImage.size() };
		result.setDevicePixelRatio(aImage.devicePixelRatio());
		result.fill(Qt::transparent);
		auto painter = QPainter{ &result };
		painter.drawPixmap(QPointF{ 0.0, 0.0 }, aImage);
		painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
		painter.fillRect(QRectF{ QPointF{ 0.0, 0.0 }, aImage.deviceIndependentSize() }, aInk);
		painter.end();
		return result;
	}

	void paint_button_arrow(QPainter& aPainter, const QRectF& aPlace, const Qt::ArrowType aArrow, const QColor& aInk)
	{
		if (aArrow == Qt::NoArrow) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.setPen(QPen{ aInk, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
		aPainter.setBrush(Qt::NoBrush);
		const auto centre  = aPlace.center();
		const auto reach   = std::min(aPlace.width(), aPlace.height()) / 4.0;
		auto       chevron = QPolygonF{ };
		switch (aArrow) {
		case Qt::DownArrow:
			chevron << QPointF{ centre.x() - reach, centre.y() - reach / 2.0 } << QPointF{ centre.x(), centre.y() + reach / 2.0 } << QPointF{ centre.x() + reach, centre.y() - reach / 2.0 };
			break;
		case Qt::UpArrow:
			chevron << QPointF{ centre.x() - reach, centre.y() + reach / 2.0 } << QPointF{ centre.x(), centre.y() - reach / 2.0 } << QPointF{ centre.x() + reach, centre.y() + reach / 2.0 };
			break;
		case Qt::LeftArrow:
			chevron << QPointF{ centre.x() + reach / 2.0, centre.y() - reach } << QPointF{ centre.x() - reach / 2.0, centre.y() } << QPointF{ centre.x() + reach / 2.0, centre.y() + reach };
			break;
		default:
			chevron << QPointF{ centre.x() - reach / 2.0, centre.y() - reach } << QPointF{ centre.x() + reach / 2.0, centre.y() } << QPointF{ centre.x() - reach / 2.0, centre.y() + reach };
			break;
		}
		aPainter.drawPolyline(chevron);
		aPainter.restore();
	}
} // namespace bpr::qt
