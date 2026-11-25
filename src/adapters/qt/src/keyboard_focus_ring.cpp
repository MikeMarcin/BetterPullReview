#include "bpr/qt/keyboard_focus_ring.hpp"

#include "bpr/core/appearance_contrast.hpp"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QEvent>
#include <QFocusEvent>
#include <QHash>
#include <QPainter>
#include <QPen>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		auto channels(const QColor& aColour) -> core::appearance_color
		{
			return { static_cast<std::uint8_t>(aColour.red()), static_cast<std::uint8_t>(aColour.green()), static_cast<std::uint8_t>(aColour.blue()), 255U };
		}

		/// Decoration: present to accessibility only as invisible, so a screen
		/// reader neither lands on the ring nor reads it as a group.
		class focus_ring_accessible final : public QAccessibleWidget
		{
		public:
			explicit focus_ring_accessible(QWidget* const aRing)
				: QAccessibleWidget{ aRing, QAccessible::NoRole }
			{
			}

			[[nodiscard]] auto state() const -> QAccessible::State override
			{
				auto state      = QAccessibleWidget::state();
				state.invisible = true;
				state.focusable = false;
				return state;
			}
		};

		auto focus_ring_accessible_factory(const QString& aKey, QObject* const aObject) -> QAccessibleInterface*
		{
			static_cast<void>(aKey);
			if (auto* const ring = dynamic_cast<keyboard_focus_ring*>(aObject); ring != nullptr) {
				return new focus_ring_accessible{ ring };
			}
			return nullptr;
		}

		void install_focus_ring_accessibility()
		{
			static auto installed = false;
			if (!installed) {
				QAccessible::installFactory(&focus_ring_accessible_factory);
				installed = true;
			}
		}

		/// Whether `aWidget` is part of `aRoot`'s own surface, not a popup or
		/// another window parented under it.
		auto on_surface(const QWidget& aRoot, const QWidget* aWidget) -> bool
		{
			for (; aWidget != nullptr && aWidget != &aRoot; aWidget = aWidget->parentWidget()) {
				if (aWidget->isWindow()) {
					return false;
				}
			}
			return aWidget == &aRoot;
		}
	}

	auto focus_ring_colour(const QPalette& aPalette, const QPalette::ColorRole aSurface) -> QColor
	{
		// Few pairs appear on a page and a paint asks for one many times.
		static auto known  = QHash<std::pair<QRgb, QRgb>, QColor>{ };
		const auto  accent = aPalette.color(QPalette::Active, QPalette::Highlight);
		const auto  ground = aPalette.color(QPalette::Active, aSurface);
		const auto  key    = std::pair{ accent.rgb(), ground.rgb() };
		if (const auto found = known.constFind(key); found != known.cend()) {
			return *found;
		}
		const auto ring = core::visible_focus_indicator(channels(accent), channels(ground));
		return *known.insert(key, QColor{ ring.Red, ring.Green, ring.Blue });
	}

	keyboard_focus_ring::keyboard_focus_ring(QWidget& aControl, const QString& aObjectName)
		: QWidget{ &aControl }
	{
		install_focus_ring_accessibility();
		setObjectName(aObjectName);
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAttribute(Qt::WA_NoSystemBackground);
		setFocusPolicy(Qt::NoFocus);
		setGeometry(aControl.rect());
		setVisible(aControl.hasFocus());
		aControl.installEventFilter(this);
	}

	auto keyboard_focus_ring::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		switch (aEvent->type()) {
		case QEvent::FocusIn:
			// A focus-in can arrive after focus has already moved on, with no
			// focus-out to follow, as when a sheet reopens and hands focus to
			// its default control ([[BPR-1161]]). The control's own focus decides.
			setGeometry(parentWidget()->rect());
			setVisible(parentWidget()->hasFocus());
			raise();
			break;
		case QEvent::FocusOut:
			// A menu opened from the control leaves focus where it was.
			if (static_cast<QFocusEvent*>(aEvent)->reason() != Qt::PopupFocusReason) {
				hide();
			}
			break;
		case QEvent::Resize: setGeometry(parentWidget()->rect()); break;
		// A window hidden with focus inside gives no focus-out; shown again, the
		// ring shows only on the control that has focus then ([[BPR-1161]]).
		case QEvent::Show: setVisible(parentWidget()->hasFocus()); break;
		default:           break;
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void keyboard_focus_ring::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		const auto& control = *parentWidget();
		auto        painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen{ focus_ring_colour(control.palette(), control.backgroundRole()), focus_ring_width });
		constexpr auto inset = focus_ring_width / 2.0;
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(inset, inset, -inset, -inset), 3.0, 3.0);
	}

	auto text_focus_boxes(const QTextEdit& aReader, const int aFirst, const int aLast) -> std::vector<QRectF>
	{
		auto boxes  = std::vector<QRectF>{ };
		auto cursor = QTextCursor{ aReader.document() };
		for (auto block = aReader.document()->findBlock(aFirst); block.isValid() && block.position() < aLast; block = block.next()) {
			const auto* const layout = block.layout();
			for (auto index = 0; block.isVisible() && layout != nullptr && index < layout->lineCount(); ++index) {
				const auto line  = layout->lineAt(index);
				const auto first = std::max(aFirst - block.position(), line.textStart());
				const auto last  = std::min(aLast - block.position(), line.textStart() + line.textLength());
				if (first >= last) {
					continue;
				}
				// The reader says where the line stands on its viewport, in
				// whole pixels; the line says how far its words run.
				cursor.setPosition(block.position() + first);
				const auto caret = QRectF{ aReader.cursorRect(cursor) };
				const auto run   = std::round(line.cursorToX(last) - line.cursorToX(first));
				boxes.emplace_back(caret.left() + std::min(0.0, run), caret.top(), std::abs(run), caret.height());
			}
		}
		return boxes;
	}

	void paint_text_focus_ring(QPainter& aPainter, const QTextEdit& aReader, const std::vector<QRectF>& aBoxes)
	{
		constexpr auto inset = focus_ring_width / 2.0;
		// Beside the words the ring's middle is two pixels out: its stroke
		// then stands in the middle of the space between two words, clear of
		// the link's last letter and of the next word's first.
		constexpr auto aside = 2.0;
		constexpr auto above = 1.0;
		const auto     edge  = QRectF{ aReader.viewport()->rect() };
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.setBrush(Qt::NoBrush);
		aPainter.setPen(QPen{ focus_ring_colour(aReader.palette(), QPalette::Base), focus_ring_width });
		for (const auto& words : aBoxes) {
			// A reader in place has no margin: its first word starts at the
			// viewport's edge and its first line at the top. A side with no
			// room beyond the words is brought to the edge. Above and below,
			// a line has room over its letters, and the side is drawn whole
			// inside the edge. Beside them it has none: the side is drawn on
			// the edge, where half its width shows and no letter is drawn
			// over. Words scrolled part out of view stay cut.
			auto ring = words.adjusted(-aside, -above, aside, above);
			if (words.left() >= edge.left()) {
				ring.setLeft(std::max(ring.left(), edge.left()));
			}
			if (words.top() >= edge.top()) {
				ring.setTop(std::max(ring.top(), edge.top() + inset));
			}
			if (words.right() <= edge.right()) {
				ring.setRight(std::min(ring.right(), edge.right()));
			}
			if (words.bottom() <= edge.bottom()) {
				ring.setBottom(std::min(ring.bottom(), edge.bottom() - inset));
			}
			aPainter.drawRoundedRect(ring, 3.0, 3.0);
		}
		aPainter.restore();
	}

	void ring_keyboard_focus(QWidget& aRoot, const QString& aObjectName, const QWidget* const aExcept)
	{
		for (auto* const control : aRoot.findChildren<QWidget*>()) {
			if ((control->focusPolicy() & Qt::TabFocus) == 0 || dynamic_cast<keyboard_focus_ring*>(control) != nullptr || !on_surface(aRoot, control)) {
				continue;
			}
			if (aExcept != nullptr && (control == aExcept || aExcept->isAncestorOf(control))) {
				continue;
			}
			if (control->findChild<QWidget*>(aObjectName, Qt::FindDirectChildrenOnly) == nullptr) {
				new keyboard_focus_ring{ *control, aObjectName };
			}
		}
	}
}
