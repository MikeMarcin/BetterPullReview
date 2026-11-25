#include "menu_row.hpp"

#include <QAction>
#include <QApplication>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionMenuItem>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace bpr::qt::detail {
	namespace {
		/// An item of `aMenu` as the style is asked to draw or measure it.
		[[nodiscard]] auto item_option(const QWidget& aMenu, const QString& aText) -> QStyleOptionMenuItem
		{
			auto option = QStyleOptionMenuItem{ };
			option.initFrom(&aMenu);
			option.state                 = QStyle::State_Enabled | QStyle::State_Active;
			option.menuItemType          = QStyleOptionMenuItem::Normal;
			option.checkType             = QStyleOptionMenuItem::NonExclusive;
			option.checked               = false;
			option.menuHasCheckableItems = true;
			option.font                  = aMenu.font();
			option.fontMetrics           = aMenu.fontMetrics();
			option.text                  = aText;
			option.maxIconWidth          = 0;
			option.reservedShortcutWidth = 0;
			return option;
		}

		[[nodiscard]] auto item_size(const QWidget& aMenu, const QString& aText) -> QSize
		{
			const auto option  = item_option(aMenu, aText);
			const auto metrics = aMenu.fontMetrics();
			return aMenu.style()->sizeFromContents(QStyle::CT_MenuItem, &option, QSize{ metrics.horizontalAdvance(aText), metrics.height() }, &aMenu);
		}
	}

	auto menu_item_height(const QWidget& aMenu) -> int
	{
		return std::max(item_size(aMenu, QStringLiteral("Xg")).height(), aMenu.fontMetrics().height() + 4);
	}

	auto menu_text_inset(const QWidget& aMenu) -> int
	{
		// The style draws an item with and without a word; where the two
		// drawings first differ is where it starts the word.
		constexpr auto fallback = 20;
		const auto     size     = item_size(aMenu, QStringLiteral("H")).expandedTo(QSize{ 96, 16 });
		const auto     draw     = [&](const QString& aText) {
			auto image = QImage{ size, QImage::Format_ARGB32_Premultiplied };
			image.fill(Qt::transparent);
			auto painter = QPainter{ &image };
			auto option  = item_option(aMenu, aText);
			option.rect  = QRect{ QPoint{ 0, 0 }, size };
			aMenu.style()->drawControl(QStyle::CE_MenuItem, &option, &painter, &aMenu);
			painter.end();
			return image;
		};
		const auto blank = draw(QString{ });
		const auto word  = draw(QStringLiteral("H"));
		for (auto x = 0; x < size.width(); ++x) {
			for (auto y = 0; y < size.height(); ++y) {
				if (blank.pixel(x, y) != word.pixel(x, y)) {
					// The glyph's own left bearing is a pixel or so.
					return std::max(0, x - 1);
				}
			}
		}
		return fallback;
	}

	menu_row::menu_row(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover);
		setMouseTracking(true);
	}

	void menu_row::set_ticked(const bool aTicked)
	{
		if (Ticked != aTicked) {
			Ticked = aTicked;
			update();
		}
	}

	void menu_row::set_activation(std::function<void()> aActivate)
	{
		Activate = std::move(aActivate);
		setCursor(Activate ? Qt::PointingHandCursor : Qt::ArrowCursor);
	}

	void menu_row::set_action(QAction* const aAction) { Action = aAction; }

	void menu_row::set_highlight_observer(std::function<void(bool)> aObserver) { Observer = std::move(aObserver); }

	void menu_row::add_control(QWidget& aControl)
	{
		Controls.push_back(&aControl);
		aControl.installEventFilter(this);
	}

	void menu_row::set_plain(const bool aPlain)
	{
		if (Plain != aPlain) {
			Plain = aPlain;
			refresh();
		}
	}

	auto menu_row::menu() const -> QMenu*
	{
		for (auto* parent = parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			if (auto* const found = qobject_cast<QMenu*>(parent); found != nullptr) {
				return found;
			}
		}
		return nullptr;
	}

	auto menu_row::highlighted() const -> bool
	{
		if (Plain || !isEnabled()) {
			return false;
		}
		if (underMouse()) {
			return true;
		}
		const auto* const focus = QApplication::focusWidget();
		return focus != nullptr && (focus == this || isAncestorOf(focus));
	}

	auto menu_row::ink() const -> QColor
	{
		const auto* const from = menu() != nullptr ? static_cast<const QWidget*>(menu()) : this;
		return from->palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, Lit ? QPalette::HighlightedText : QPalette::Text);
	}

	auto menu_row::ground() const -> QColor
	{
		const auto* const from = menu() != nullptr ? static_cast<const QWidget*>(menu()) : this;
		return from->palette().color(QPalette::Active, Lit ? QPalette::Highlight : QPalette::Window);
	}

	auto menu_row::sizeHint() const -> QSize
	{
		const auto inner = layout() != nullptr ? layout()->sizeHint() : QSize{ };
		return QSize{ inner.width(), std::max(inner.height(), minimumSizeHint().height()) };
	}

	auto menu_row::minimumSizeHint() const -> QSize
	{
		const auto* const from  = menu() != nullptr ? static_cast<const QWidget*>(menu()) : this;
		const auto        inner = layout() != nullptr ? layout()->minimumSize() : QSize{ };
		return QSize{ inner.width(), std::max(inner.height(), menu_item_height(*from)) };
	}

	void menu_row::refresh()
	{
		const auto lit = highlighted();
		if (lit && underMouse() && Action != nullptr) {
			// The menu's own highlight leaves the item the pointer came from.
			if (auto* const owner = menu(); owner != nullptr && owner->activeAction() != Action) {
				owner->setActiveAction(Action);
			}
		}
		if (Lit != lit) {
			Lit = lit;
			// For courses, and for a style that wants to know.
			setProperty("highlighted", lit);
			if (Observer) {
				Observer(lit);
			}
		}
		update();
	}

	void menu_row::paintEvent(QPaintEvent* /*aEvent*/)
	{
		const auto* const from    = menu() != nullptr ? static_cast<const QWidget*>(menu()) : this;
		auto              painter = QPainter{ this };
		auto              option  = item_option(*from, QString{ });
		option.rect               = rect();
		option.checked            = Ticked;
		if (!isEnabled()) {
			option.state &= ~QStyle::State_Enabled;
			option.palette.setCurrentColorGroup(QPalette::Disabled);
		}
		if (Lit) {
			option.state |= QStyle::State_Selected;
		}
		from->style()->drawControl(QStyle::CE_MenuItem, &option, &painter, from);
	}

	void menu_row::enterEvent(QEnterEvent* const aEvent)
	{
		QWidget::enterEvent(aEvent);
		refresh();
	}

	void menu_row::leaveEvent(QEvent* const aEvent)
	{
		QWidget::leaveEvent(aEvent);
		refresh();
	}

	void menu_row::focusInEvent(QFocusEvent* const aEvent)
	{
		QWidget::focusInEvent(aEvent);
		refresh();
	}

	void menu_row::focusOutEvent(QFocusEvent* const aEvent)
	{
		QWidget::focusOutEvent(aEvent);
		refresh();
	}

	void menu_row::mouseReleaseEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() == Qt::LeftButton && Activate && rect().contains(aEvent->position().toPoint())) {
			aEvent->accept();
			Activate();
			return;
		}
		QWidget::mouseReleaseEvent(aEvent);
	}

	void menu_row::step(const int aBy)
	{
		// The row, then each of its controls that can be used now.
		auto stops = QList<QWidget*>{ this };
		for (const auto& control : Controls) {
			if (control != nullptr && control->isVisibleTo(this) && control->isEnabled() && control->focusPolicy() != Qt::NoFocus) {
				stops.push_back(control.data());
			}
		}
		const auto* const focus = QApplication::focusWidget();
		auto              place = 0;
		for (auto index = 0; index < stops.size(); ++index) {
			if (stops[index] == focus) {
				place = index;
			}
		}
		const auto next = std::clamp(place + aBy, 0, static_cast<int>(stops.size()) - 1);
		if (next != place) {
			stops[next]->setFocus(Qt::TabFocusReason);
		}
	}

	void menu_row::keyPressEvent(QKeyEvent* const aEvent)
	{
		const auto forward = isRightToLeft() ? Qt::Key_Left : Qt::Key_Right;
		const auto back    = isRightToLeft() ? Qt::Key_Right : Qt::Key_Left;
		if ((aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter || aEvent->key() == Qt::Key_Space) && Activate && hasFocus()) {
			aEvent->accept();
			Activate();
			return;
		}
		if (aEvent->key() == forward || aEvent->key() == back) {
			aEvent->accept();
			step(aEvent->key() == forward ? 1 : -1);
			return;
		}
		// Up, Down and Esc are the menu's.
		QWidget::keyPressEvent(aEvent);
	}

	void menu_row::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::EnabledChange) {
			refresh();
		}
	}

	auto menu_row::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut) {
			// Once the focus has moved: the row is lit while a control holds it.
			QTimer::singleShot(0, this, [this] { refresh(); });
		}
		if (aEvent->type() == QEvent::KeyPress) {
			// A button moves the keyboard among its siblings with the arrows;
			// in a row they are the row's and the menu's instead.
			switch (static_cast<QKeyEvent*>(aEvent)->key()) {
			case Qt::Key_Left:
			case Qt::Key_Right:
			case Qt::Key_Up:
			case Qt::Key_Down:
				QApplication::sendEvent(this, aEvent);
				return true;
			default:
				break;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
}
