#ifndef BPR_SRC_ADAPTERS_QT_SRC_LIST_ROW_HANDLES_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_LIST_ROW_HANDLES_HPP

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

class QListWidget;
class QToolButton;

namespace bpr::qt::detail {
	/// \brief [[BPR-2318]]. The width of the gutter at a row's left where its
	/// grip sits; a delegate starts the row's own content after it.
	inline constexpr int row_grip_gutter = 20;
	/// \brief [[BPR-2318]]. The room a row's trash takes at its right.
	inline constexpr int row_trash_room = 34;

	/// \brief [[BPR-2318]] (owner, 2026-11-20: "Agents should have drag
	/// handles to reorder how they appear"; "There should be a visible
	/// trashcan on each row"). A grip at the left of every row of a list and,
	/// when the list's rows can be removed, a trash button at its right.
	///
	/// \details Presentation only: what a move or a removal does is the
	/// owner's, through `hooks`. The grip is drawn over the row and lets the
	/// mouse through, so pressing it starts the list's own drag; it carries
	/// the row's accessible name ("Reorder Work Claude") and says how to move
	/// it from the keyboard. With the list focused, Alt+Up and Alt+Down move
	/// the current row, and Delete or Backspace removes it as its trash does.
	/// The handles follow the rows as they are added, removed, moved, renamed
	/// or scrolled.
	class list_row_handles final : public QObject
	{
	public:
		struct hooks
		{
			/// \brief The row's name as read aloud, in "Reorder %1" and
			/// "Remove %1".
			std::function<QString(int aRow)> Name;
			/// \brief Moves the row one place: -1 up, 1 down.
			std::function<void(int aRow, int aDelta)> Move;
			/// \brief Removes the row, or asks first; none: no trash, and
			/// Delete does nothing.
			std::function<void(int aRow)> Remove;
			/// \brief The trash buttons' object name, for tests and automation.
			QString TrashName;
		};

		list_row_handles(QListWidget& aList, hooks aHooks);

		/// \brief Matches the handles to the rows, their names and places.
		void sync();
		/// \brief The row's grip, or null.
		[[nodiscard]] auto grip(int aRow) const -> QWidget*;
		/// \brief The row's trash, or null when the rows cannot be removed.
		[[nodiscard]] auto trash(int aRow) const -> QToolButton*;

	protected:
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override;

	private:
		void place();
		void shade();

		QListWidget&              List;
		hooks                     Hooks;
		QPointer<QWidget>         Viewport;
		std::vector<QWidget*>     Grips;
		std::vector<QToolButton*> Trash;
	};
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_LIST_ROW_HANDLES_HPP
