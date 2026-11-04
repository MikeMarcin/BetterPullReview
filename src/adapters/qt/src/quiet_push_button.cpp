#include "bpr/qt/quiet_push_button.hpp"

#include "button_surface.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>

#include <algorithm>

namespace bpr::qt {
	quiet_push_button::quiet_push_button(QWidget* const aParent)
		: QPushButton{ aParent }
	{
		setAttribute(Qt::WA_Hover, true);
	}

	quiet_push_button::quiet_push_button(const QString& aText, QWidget* const aParent)
		: quiet_push_button{ aParent }
	{
		setText(aText);
	}

	auto quiet_push_button::uses_chip() const -> bool
	{
		return !text().isEmpty() && !isDefault() && menu() == nullptr;
	}

	auto quiet_push_button::sizeHint() const -> QSize
	{
		const auto native = QPushButton::sizeHint();
		if (!uses_chip()) {
			return native;
		}
		// [[BPR-1855]]. The words in the button's own font, rounded up, inside the
		// chip's padding: the same geometry paint_button_label draws in.
		const auto content = chip_content_size(*this, iconSize(), !icon().isNull());
		return QSize{ (2 * chip_padding) + content.width(), std::max(native.height(), content.height() + 6) };
	}

	auto quiet_push_button::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void quiet_push_button::focusInEvent(QFocusEvent* const aEvent)
	{
		update_button_keyboard_focus(KeyboardFocus, *aEvent);
		QPushButton::focusInEvent(aEvent);
	}

	void quiet_push_button::changeEvent(QEvent* const aEvent)
	{
		QPushButton::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::PaletteChange) {
			updateGeometry();
			update();
		}
	}

	void quiet_push_button::paintEvent(QPaintEvent* const aEvent)
	{
		if (!uses_chip()) {
			QPushButton::paintEvent(aEvent);
			return;
		}
		auto painter = QPainter{ this };
		paint_button_surface(painter, *this, isChecked() || (!isCheckable() && !isFlat()), KeyboardFocus);
		paint_button_label(painter, *this, iconSize(), !icon().isNull());
	}
} // namespace bpr::qt
