#include "bpr/qt/info_glyph.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QPoint>
#include <QToolTip>

#include <utility>

namespace bpr::qt {
	info_glyph::info_glyph(QString aConsequence, QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setObjectName(QStringLiteral("infoGlyph"));
		setAutoRaise(true);
		setFocusPolicy(Qt::StrongFocus);
		set_consequence(std::move(aConsequence));
		apply_glyph();
		connect(this, &QToolButton::clicked, this, [this] { show_popover(); });
	}

	void info_glyph::set_consequence(QString aConsequence)
	{
		Consequence = std::move(aConsequence);
		// Hovering shows the tooltip, which is the popover; focus and a press
		// show the same sentence through `show_popover`.
		setToolTip(Consequence);
		setAccessibleDescription(Consequence);
	}

	auto info_glyph::consequence() const noexcept -> const QString&
	{
		return Consequence;
	}

	void info_glyph::show_popover()
	{
		QToolTip::showText(mapToGlobal(QPoint{ 0, height() }), Consequence, this, rect());
	}

	void info_glyph::focusInEvent(QFocusEvent* const aEvent)
	{
		QToolButton::focusInEvent(aEvent);
		// A keyboard reviewer reaches the sentence by focusing the glyph; a
		// pointer that clicked already sees it through the press.
		if (aEvent->reason() != Qt::MouseFocusReason && aEvent->reason() != Qt::PopupFocusReason) {
			show_popover();
		}
	}

	void info_glyph::focusOutEvent(QFocusEvent* const aEvent)
	{
		QToolButton::focusOutEvent(aEvent);
		if (QToolTip::isVisible() && QToolTip::text() == Consequence) {
			QToolTip::hideText();
		}
	}

	void info_glyph::changeEvent(QEvent* const aEvent)
	{
		QToolButton::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			apply_glyph();
		}
	}

	void info_glyph::apply_glyph()
	{
		const auto icon = QIcon::fromTheme(QIcon::ThemeIcon::DialogInformation);
		if (!icon.isNull()) {
			setIcon(icon);
			setText({ });
			setToolButtonStyle(Qt::ToolButtonIconOnly);
			return;
		}
		// U+24D8 CIRCLED LATIN SMALL LETTER I, where the font has it.
		const auto glyph = QChar{ 0x24D8 };
		setIcon(QIcon{ });
		setText(QFontMetrics{ font() }.inFont(glyph) ? QString{ glyph } : QStringLiteral("i"));
		setToolButtonStyle(Qt::ToolButtonTextOnly);
	}
} // namespace bpr::qt
