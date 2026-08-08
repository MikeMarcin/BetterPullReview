#include "bpr/qt/menu_tool_button.hpp"

#include <QStyle>
#include <QStyleOptionToolButton>

namespace bpr::qt {
	auto menu_tool_button::with_indicator(const QSize aHint) const -> QSize
	{
		if (menu() == nullptr || popupMode() != QToolButton::InstantPopup) {
			// Every other mode either draws no indicator or already reserves
			// one. Widening those would push the label off centre to correct a
			// defect they do not have.
			return aHint;
		}
		auto option = QStyleOptionToolButton{ };
		option.initFrom(this);
		option.text          = text();
		option.icon          = icon();
		option.features      = QStyleOptionToolButton::HasMenu;
		const auto indicator = style()->pixelMetric(
			QStyle::PM_MenuButtonIndicator, &option, this);
		return QSize{ aHint.width() + indicator, aHint.height() };
	}

	auto menu_tool_button::sizeHint() const -> QSize
	{
		return with_indicator(QToolButton::sizeHint());
	}

	auto menu_tool_button::minimumSizeHint() const -> QSize
	{
		// Deliberately this override and not `with_indicator` again.
		// `QToolButton::minimumSizeHint` is defined as its size hint, so it
		// calls the override above and the width already carries the
		// indicator; widening it a second time made the button sixteen pixels
		// broader than it asked to be, which a first draft of this did.
		//
		// The minimum still matters — a layout under pressure squeezes towards
		// it, and a minimum without the indicator would reproduce the overflow
		// exactly when the window is tight. It gets that by inheriting this
		// class's size hint rather than by a second correction.
		return sizeHint();
	}
} // namespace bpr::qt
