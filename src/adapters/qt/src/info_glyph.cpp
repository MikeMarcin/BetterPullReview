#include "bpr/qt/info_glyph.hpp"

#include "bpr/qt/anchored_tooltip.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QMetaObject>

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
		// Held by focus, not by the pointer. A tip that hides as soon as the
		// pointer leaves the glyph is right for a tip the pointer summoned and
		// wrong for this one: a keyboard reviewer's pointer is somewhere else
		// entirely, or nowhere, so that form showed the sentence to nobody and
		// the focus route quietly reached no one ([[BPR-0790]]). Hovering still
		// shows the same sentence through the tooltip, and RFC-BPR-0034's row
		// language requires both routes to work. Either way it sits right
		// below the glyph ([[BPR-1354]]).
		show_anchored_tooltip(*this, rect(), Consequence, anchored_tooltip_hold::focus);
	}

	void info_glyph::focusInEvent(QFocusEvent* const aEvent)
	{
		QToolButton::focusInEvent(aEvent);
		// A keyboard reviewer reaches the sentence by focusing the glyph; a
		// pointer that clicked already sees it through the press.
		if (aEvent->reason() != Qt::MouseFocusReason && aEvent->reason() != Qt::PopupFocusReason) {
			// Deferred by one turn of the event loop, which is load-bearing.
			// Qt's tip window hides itself on FocusIn and FocusOut, so a tip
			// shown from inside this handler is destroyed by the very focus
			// change that asked for it: the sentence was never shown to the
			// keyboard reviewer it exists for, while every property a test can
			// read still looked correct ([[BPR-0790]]).
			QMetaObject::invokeMethod(
				this,
				[this] {
					if (hasFocus()) {
						show_popover();
					}
				},
				Qt::QueuedConnection);
		}
	}

	void info_glyph::focusOutEvent(QFocusEvent* const aEvent)
	{
		QToolButton::focusOutEvent(aEvent);
		if (const auto* const tip = anchored_tooltip(); tip != nullptr && tip->parentWidget() == this) {
			hide_anchored_tooltip();
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
