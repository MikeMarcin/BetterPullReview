#include "bpr/qt/quiet_tool_button.hpp"

#include "button_surface.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionToolButton>

#include <algorithm>

namespace bpr::qt {
	quiet_tool_button::quiet_tool_button(QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setAttribute(Qt::WA_Hover, true);
	}

	auto quiet_tool_button::uses_chip() const -> bool
	{
		const auto textStyle = toolButtonStyle() == Qt::ToolButtonTextOnly || toolButtonStyle() == Qt::ToolButtonTextBesideIcon;
		return !text().isEmpty() && menu() == nullptr && arrowType() == Qt::NoArrow && toolButtonStyle() != Qt::ToolButtonTextUnderIcon && (icon().isNull() || textStyle);
	}

	auto quiet_tool_button::shows_icon() const -> bool
	{
		return !icon().isNull() && toolButtonStyle() == Qt::ToolButtonTextBesideIcon;
	}

	auto quiet_tool_button::sizeHint() const -> QSize
	{
		if (!uses_chip()) {
			return QToolButton::sizeHint();
		}
		auto option = QStyleOptionToolButton{ };
		initStyleOption(&option);
		// [[BPR-1855]]. The same content geometry paint_button_label draws in.
		const auto content     = chip_content_size(*this, option.iconSize, shows_icon());
		const auto across      = (2 * chip_padding) + content.width();
		option.toolButtonStyle = Qt::ToolButtonTextBesideIcon;
		const auto contents    = QSize{ across, std::max(option.iconSize.height(), content.height()) };
		const auto tall        = style()->sizeFromContents(QStyle::CT_ToolButton, &option, contents, this).height();
		return QSize{ across, std::max(tall, content.height() + 6) };
	}

	auto quiet_tool_button::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void quiet_tool_button::focusInEvent(QFocusEvent* const aEvent)
	{
		update_button_keyboard_focus(KeyboardFocus, *aEvent);
		QToolButton::focusInEvent(aEvent);
	}

	void quiet_tool_button::changeEvent(QEvent* const aEvent)
	{
		QToolButton::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::PaletteChange) {
			updateGeometry();
			update();
		}
	}

	void quiet_tool_button::paintEvent(QPaintEvent* const aEvent)
	{
		if (!uses_chip()) {
			QToolButton::paintEvent(aEvent);
			return;
		}
		auto painter = QPainter{ this };
		paint_button_surface(painter, *this, isChecked() || (!isCheckable() && !autoRaise()), KeyboardFocus);
		// With no icon, Qt's default IconOnly still means a text button.
		// [[BPR-1855]]. The chip draws its own content in the room it measured;
		// the style's label element may elide or inset it.
		auto option = QStyleOptionToolButton{ };
		initStyleOption(&option);
		paint_button_label(painter, *this, option.iconSize, shows_icon());
	}
} // namespace bpr::qt
