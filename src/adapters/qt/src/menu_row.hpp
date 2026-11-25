#ifndef BPR_SRC_ADAPTERS_QT_SRC_MENU_ROW_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MENU_ROW_HPP

#include <QColor>
#include <QPointer>
#include <QWidget>

#include <functional>

class QAction;
class QMenu;

/// \file
/// \brief A row of widgets inside a menu that reads as one of the menu's own
/// items (owner, attended review 2026-11-24: "there no highlight on the mouse
/// over for the session rows").
/// \details A `QWidgetAction`'s widget covers its item, so the menu paints
/// neither its highlight nor its tick there. A `menu_row` has the style draw
/// both, as it draws the items around it (`QStyle::CE_MenuItem`): highlighted
/// under the pointer and while the keyboard is in it, ticked when asked, as
/// tall as an item, its words starting where an item's start. Presentation
/// only.
namespace bpr::qt::detail {
	/// \brief Where the style starts a menu item's words, from the item's
	/// left edge. Measured from the style's own drawing of an item, so it
	/// holds in every style.
	[[nodiscard]] auto menu_text_inset(const QWidget& aMenu) -> int;

	/// \brief How tall the style makes a one-line item of \p aMenu.
	[[nodiscard]] auto menu_item_height(const QWidget& aMenu) -> int;

	class menu_row : public QWidget
	{
	public:
		explicit menu_row(QWidget* aParent);

		/// \brief The row's item carries the menu's tick.
		void               set_ticked(bool aTicked);
		[[nodiscard]] auto ticked() const noexcept -> bool { return Ticked; }
		/// \brief What a click on the row (outside its own controls), Return
		/// and Space do; none makes the row inert.
		void set_activation(std::function<void()> aActivate);
		/// \brief The menu action the row belongs to: while the pointer is in
		/// the row it is the menu's active one, so no other item stays lit.
		void set_action(QAction* aAction);
		/// \brief Told when the highlight comes or goes, to dress the row's
		/// widgets for it (`ink()` and `ground()` are then the highlight's).
		void set_highlight_observer(std::function<void(bool)> aObserver);
		/// \brief A control of the row: the row is highlighted while it holds
		/// the keyboard, and Left and Right move between the row and it.
		void add_control(QWidget& aControl);
		/// \brief A row that is not highlighted whatever happens (its name is
		/// being edited in place).
		void set_plain(bool aPlain);

		[[nodiscard]] auto highlighted() const -> bool;
		/// \brief The colours the row's words and marks are read in now, and
		/// the colour behind them while highlighted.
		[[nodiscard]] auto ink() const -> QColor;
		[[nodiscard]] auto ground() const -> QColor;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void focusOutEvent(QFocusEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		[[nodiscard]] auto menu() const -> QMenu*;
		void               refresh();
		void               step(int aBy);

		std::function<void()>     Activate;
		std::function<void(bool)> Observer;
		QPointer<QAction>         Action;
		QList<QPointer<QWidget>>  Controls;
		bool                      Ticked = false;
		bool                      Plain  = false;
		bool                      Lit    = false;
	};
}

#endif
