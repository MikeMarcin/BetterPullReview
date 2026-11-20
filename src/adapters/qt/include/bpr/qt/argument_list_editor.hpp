#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ARGUMENT_LIST_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ARGUMENT_LIST_EDITOR_HPP

#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class QListWidget;
class QPushButton;
class QToolButton;

namespace bpr::qt {
	namespace detail {
		class list_row_handles;
	}

	/// \brief [[BPR-2318]] (owner, 2026-11-20: "Arguments in the agent
	/// settings isn't good, needs to show more than one row, needs drag
	/// handles, needs scroll bar"). An agent's fixed arguments, one per row.
	///
	/// \details Shows at least five rows and scrolls past them. Each row has a
	/// grip to drag it to another place and a trash to remove it; with the
	/// list focused Alt+Up and Alt+Down move the current row and Delete
	/// removes it. **Add argument** adds a row and starts editing it;
	/// double-clicking a row, or the platform's edit key, edits it. A row left
	/// empty is removed. Each argument is kept as written, trimmed at its
	/// ends; nothing splits or quotes it.
	class argument_list_editor final : public QWidget
	{
	public:
		explicit argument_list_editor(QWidget* aParent = nullptr);

		/// \brief Shows `aArguments`, one per row, in order.
		void set_arguments(const std::vector<std::string>& aArguments);
		/// \brief The rows as they stand, in order, trimmed, without empty ones.
		[[nodiscard]] auto arguments() const -> std::vector<std::string>;
		/// \brief Called after any change to the rows.
		void set_change(std::function<void()> aChange);
		/// \brief Adds a row at the end and starts editing it, as Add argument does.
		void add_row();

		[[nodiscard]] auto list() const noexcept -> QListWidget* { return List; }
		[[nodiscard]] auto add_button() const noexcept -> QPushButton* { return Add; }
		/// \brief The row's grip, or null.
		[[nodiscard]] auto grip(int aRow) const -> QWidget*;
		/// \brief The row's trash, or null.
		[[nodiscard]] auto trash(int aRow) const -> QToolButton*;

	private:
		void changed();
		void drop_empty();
		void move(int aRow, int aDelta);

		std::function<void()>     Change;
		QListWidget*              List    = nullptr;
		QPushButton*              Add     = nullptr;
		detail::list_row_handles* Handles = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ARGUMENT_LIST_EDITOR_HPP
