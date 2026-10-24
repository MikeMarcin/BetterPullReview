#include "bpr/qt/quiet_tool_button.hpp"

#include "button_surface.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionToolButton>

#include <algorithm>

namespace bpr::qt {
	namespace {
		constexpr int chip_padding = 6;
		constexpr int icon_gap     = 4;
	} // namespace

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

	auto quiet_tool_button::sizeHint() const -> QSize
	{
		if (!uses_chip()) {
			return QToolButton::sizeHint();
		}
		auto option = QStyleOptionToolButton{ };
		initStyleOption(&option);
		const auto metrics     = fontMetrics();
		const auto words       = metrics.size(Qt::TextShowMnemonic, text());
		const auto beside      = !icon().isNull() && toolButtonStyle() == Qt::ToolButtonTextBesideIcon;
		const auto across      = (2 * chip_padding) + words.width() + (beside ? option.iconSize.width() + icon_gap : 0);
		option.toolButtonStyle = Qt::ToolButtonTextBesideIcon;
		const auto contents    = QSize{ across, std::max(option.iconSize.height(), words.height()) };
		const auto tall        = style()->sizeFromContents(QStyle::CT_ToolButton, &option, contents, this).height();
		return QSize{ across, std::max(tall, words.height() + 6) };
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
		auto option = QStyleOptionToolButton{ };
		initStyleOption(&option);
		option.rect.adjust(chip_padding, 0, -chip_padding, 0);
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			option.palette.setColor(group, QPalette::ButtonText, option.palette.color(group, QPalette::WindowText));
		}
		// With no icon, Qt's default IconOnly still means a text button. State
		// remains intact so native label drawing retains mnemonics and icon modes.
		style()->drawControl(QStyle::CE_ToolButtonLabel, &option, &painter, this);
	}
} // namespace bpr::qt
