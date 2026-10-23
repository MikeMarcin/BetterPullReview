#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FOLDER_LIST_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FOLDER_LIST_EDITOR_HPP

#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class QLabel;
class QListWidget;
class QToolButton;

namespace bpr::qt {
	/// \brief [[BPR-PENDING-AQ6]], owner look v002 ("I prefer a UX more like
	/// the visual studio one"). An ordered list of folders edited in place, as
	/// Visual Studio's folder-list options are.
	///
	/// \details A toolbar above the list's right edge holds **New folder**,
	/// **Remove**, **Move down** and **Move up**. New folder adds a row and
	/// starts editing it; double-clicking or pressing F2 or Return edits a
	/// row. An editing row is a path field with a **…** button at its end
	/// that opens the folder browser. A leading `~` is the home folder; a path
	/// that is not absolute is refused with what to write and the row goes
	/// back to what it was; an emptied row is removed. Every change hands the
	/// whole ordered list to `set_change`'s observer, which asks the host; the
	/// list shown is what the host then presents.
	class folder_list_editor final : public QWidget
	{
	public:
		explicit folder_list_editor(QWidget* aParent = nullptr);

		void set_change(std::function<void(std::vector<std::string>)> aChange);
		/// Shows the host's list, unless a row is being edited.
		void present(const std::vector<std::string>& aFolders);
		/// Adds a row and starts editing it, as New folder does.
		void add_row();

		[[nodiscard]] auto list() const noexcept -> QListWidget* { return List; }
		[[nodiscard]] auto new_button() const noexcept -> QToolButton* { return New; }
		[[nodiscard]] auto remove_button() const noexcept -> QToolButton* { return Remove; }
		[[nodiscard]] auto down_button() const noexcept -> QToolButton* { return Down; }
		[[nodiscard]] auto up_button() const noexcept -> QToolButton* { return Up; }
		[[nodiscard]] auto status() const noexcept -> QLabel* { return Status; }
		/// The rows as they stand, in order, without empty ones.
		[[nodiscard]] auto folders() const -> std::vector<std::string>;

	private:
		void committed(int aRow);
		void changed();
		void update_tools();

		std::function<void(std::vector<std::string>)> Change;
		std::vector<std::string>                      Shown;
		QListWidget*                                  List    = nullptr;
		QToolButton*                                  New     = nullptr;
		QToolButton*                                  Remove  = nullptr;
		QToolButton*                                  Down    = nullptr;
		QToolButton*                                  Up      = nullptr;
		QLabel*                                       Status  = nullptr;
		bool                                          Editing = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FOLDER_LIST_EDITOR_HPP
