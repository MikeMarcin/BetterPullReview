#include "bpr/qt/quiet_push_button.hpp"

#include "button_surface.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>

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
		const auto words  = fontMetrics().size(Qt::TextShowMnemonic, text());
		const auto beside = !icon().isNull();
		return QSize{ 12 + words.width() + (beside ? iconSize().width() + 4 : 0), std::max(native.height(), words.height() + 6) };
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
		auto option = QStyleOptionButton{ };
		initStyleOption(&option);
		option.rect.adjust(6, 0, -6, 0);
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			option.palette.setColor(group, QPalette::ButtonText, option.palette.color(group, QPalette::WindowText));
		}
		style()->drawControl(QStyle::CE_PushButtonLabel, &option, &painter, this);
	}
} // namespace bpr::qt
