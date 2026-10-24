#include "button_surface.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"

#include <QAbstractButton>
#include <QFocusEvent>
#include <QPainter>

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
} // namespace bpr::qt
