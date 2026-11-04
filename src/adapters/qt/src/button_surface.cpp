#include "button_surface.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"

#include <QAbstractButton>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
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

	void paint_button_surface(QPainter& aPainter, const QAbstractButton& aButton, const bool aRaised, const bool aKeyboardFocus)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = aButton.palette();
		const auto  ground  = colours.color(QPalette::Active, QPalette::Window);
		const auto  group   = aButton.isEnabled() ? QPalette::Active : QPalette::Disabled;
		const auto  ink     = colours.color(group, QPalette::WindowText);
		const auto  box     = QRectF{ aButton.rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		const auto  pressed = aButton.isEnabled() && aButton.isDown();
		if (aRaised) {
			aPainter.setPen(QPen{ chip_border(ground, ink), 1.0 });
			aPainter.setBrush(blend(ground, ink, pressed ? 0.12 : 0.07));
			aPainter.drawRoundedRect(box, chip_radius, chip_radius);
		} else if (aButton.isEnabled() && (pressed || aButton.underMouse())) {
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(blend(ground, ink, pressed ? 0.12 : 0.05));
			aPainter.drawRoundedRect(box, chip_radius, chip_radius);
		}
		if (aButton.isEnabled() && aButton.hasFocus() && aKeyboardFocus) {
			const auto inset = focus_ring_width / 2.0;
			aPainter.setPen(QPen{ focus_ring_colour(colours, QPalette::Window), focus_ring_width });
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

	void paint_button_label(QPainter& aPainter, const QAbstractButton& aButton, const QSize aIconSize, const bool aShowIcon)
	{
		aPainter.save();
		auto* const style   = aButton.style();
		const auto  content = aButton.rect().adjusted(chip_padding, 0, -chip_padding, 0);
		const auto  extent  = chip_content_size(aButton, aIconSize, aShowIcon);
		// Centred in the padding; never starting before it when the button was
		// given less than it asked for.
		const auto left  = content.left() + std::max(0, (content.width() - extent.width()) / 2);
		auto       place = QRect{ left, content.top(), extent.width(), content.height() };
		if (aShowIcon) {
			const auto mode  = !aButton.isEnabled() ? QIcon::Disabled : (aButton.hasFocus() ? QIcon::Active : QIcon::Normal);
			const auto state = aButton.isChecked() ? QIcon::On : QIcon::Off;
			const auto image = aButton.icon().pixmap(aIconSize, aPainter.device()->devicePixelRatio(), mode, state);
			const auto spot  = QRect{ place.left(), place.top() + ((place.height() - aIconSize.height()) / 2), aIconSize.width(), aIconSize.height() };
			style->drawItemPixmap(&aPainter, QStyle::visualRect(aButton.layoutDirection(), aButton.rect(), spot), Qt::AlignCenter, image);
			place.setLeft(place.left() + aIconSize.width() + chip_icon_gap);
		}
		auto option = QStyleOption{ };
		option.initFrom(&aButton);
		auto flags = static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextSingleLine);
		if (style->styleHint(QStyle::SH_UnderlineShortcut, &option, &aButton) == 0) {
			flags |= Qt::TextHideMnemonic;
		}
		style->drawItemText(&aPainter, QStyle::visualRect(aButton.layoutDirection(), aButton.rect(), place), flags, aButton.palette(), aButton.isEnabled(),
							aButton.text(), QPalette::WindowText);
		aPainter.restore();
	}
} // namespace bpr::qt
