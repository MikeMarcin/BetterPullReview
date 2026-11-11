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
		/// The side of the chevron an icon tool shows for its menu.
		constexpr int glyph_menu_indicator = 6;
	} // namespace

	quiet_tool_button::quiet_tool_button(QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setAttribute(Qt::WA_Hover, true);
	}

	auto quiet_tool_button::shows_menu() const -> bool
	{
		auto option = QStyleOptionToolButton{ };
		initStyleOption(&option);
		return (option.features & QStyleOptionToolButton::HasMenu) != 0 && popupMode() != QToolButton::MenuButtonPopup;
	}

	auto quiet_tool_button::shows_arrow() const -> bool
	{
		return arrowType() != Qt::NoArrow;
	}

	auto quiet_tool_button::uses_glyph() const -> bool
	{
		const auto mark = !icon().isNull() || shows_arrow();
		return mark && (text().isEmpty() || toolButtonStyle() == Qt::ToolButtonIconOnly) && popupMode() != QToolButton::MenuButtonPopup;
	}

	auto quiet_tool_button::uses_chip() const -> bool
	{
		const auto textStyle = toolButtonStyle() == Qt::ToolButtonTextOnly || toolButtonStyle() == Qt::ToolButtonTextBesideIcon;
		return !text().isEmpty() && !uses_glyph() && popupMode() != QToolButton::MenuButtonPopup && toolButtonStyle() != Qt::ToolButtonTextUnderIcon
			&& ((icon().isNull() && !shows_arrow()) || textStyle);
	}

	void quiet_tool_button::set_surface(const QPalette::ColorRole aSurface)
	{
		if (Surface != aSurface) {
			Surface = aSurface;
			update();
		}
	}

	void quiet_tool_button::set_surface_colours(const QColor& aGround, const QColor& aInk)
	{
		const auto ground = aGround.isValid() && aInk.isValid() ? aGround : QColor{ };
		const auto ink    = ground.isValid() ? aInk : QColor{ };
		if (Ground != ground || Ink != ink) {
			Ground = ground;
			Ink    = ink;
			update();
		}
	}

	void quiet_tool_button::set_tinted(const bool aTinted)
	{
		if (Tinted != aTinted) {
			Tinted = aTinted;
			update();
		}
	}

	auto quiet_tool_button::shows_icon() const -> bool
	{
		return (!icon().isNull() || shows_arrow()) && toolButtonStyle() == Qt::ToolButtonTextBesideIcon;
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
		const auto across      = (2 * chip_padding) + content.width() + (shows_menu() ? chip_menu_indicator : 0);
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
		const auto glyph = uses_glyph();
		if (!glyph && !uses_chip()) {
			QToolButton::paintEvent(aEvent);
			return;
		}
		const auto colours = Ground.isValid() ? button_colours_on(palette(), Ground, Ink) : button_colours_on(palette(), Surface);
		const auto ink     = isEnabled() ? colours.Ink : colours.DisabledInk;
		const auto raised  = isChecked() || (!isCheckable() && !autoRaise());
		const auto menu    = shows_menu();
		auto       painter = QPainter{ this };
		paint_button_surface(painter, *this, colours, raised, KeyboardFocus);
		if (glyph) {
			// An icon tool keeps the style's size; only its face is ours.
			if (shows_arrow()) {
				const auto side = std::min({ width(), height(), std::max(iconSize().width(), 12) });
				paint_button_arrow(painter, QRectF{ (width() - side) / 2.0, (height() - side) / 2.0, static_cast<qreal>(side), static_cast<qreal>(side) }, arrowType(), ink);
			} else if (Tinted) {
				paint_button_glyph(painter, *this, iconSize(), ink);
			} else {
				paint_button_glyph(painter, *this, iconSize());
			}
			if (menu) {
				const auto side = static_cast<qreal>(glyph_menu_indicator);
				paint_button_menu_indicator(painter, QRectF{ width() - side - 1.0, height() - side - 1.0, side, side }, ink);
			}
			return;
		}
		// With no icon, Qt's default IconOnly still means a text button.
		// [[BPR-1855]]. The chip draws its own content in the room it measured;
		// the style's label element may elide or inset it.
		auto option = QStyleOptionToolButton{ };
		initStyleOption(&option);
		auto mark = button_mark::none;
		if (shows_icon()) {
			mark = shows_arrow() ? button_mark::arrow : (Tinted ? button_mark::tinted_icon : button_mark::icon);
		}
		paint_button_label(painter, *this, colours, option.iconSize, mark, arrowType(), menu ? chip_menu_indicator : 0);
		if (menu) {
			const auto side = static_cast<qreal>(chip_menu_indicator - chip_icon_gap);
			const auto left = layoutDirection() == Qt::RightToLeft ? static_cast<qreal>(chip_padding) : width() - chip_padding - side;
			paint_button_menu_indicator(painter, QRectF{ left, (height() - side) / 2.0, side, side }, ink);
		}
	}
} // namespace bpr::qt
