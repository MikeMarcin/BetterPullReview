#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_KEYBOARD_SHORTCUTS_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_KEYBOARD_SHORTCUTS_WINDOW_HPP

#include "bpr/qt/keyboard_shortcuts.hpp"

#include <QDialog>

#include <vector>

class QLabel;
class QLineEdit;
class QTreeWidget;

namespace bpr::qt {
	/// \brief [[BPR-1567]]. Help → Keyboard Shortcuts: every shortcut the
	/// application binds or reads, grouped by area and searchable.
	///
	/// \details The rows are generated from `shortcut_catalogue()`, the table
	/// the window's and widgets' bindings are made from, so the page lists what
	/// the application binds. Keys are drawn as key caps in the platform's
	/// notation (`QKeySequence::NativeText`); a key that acts in one place says
	/// where, and a key that a text field takes while you type says so. It is
	/// presentation only: it reads a static table and performs no effect.
	/// Non-modal; Esc and Close hide it, and its owner reuses it.
	class keyboard_shortcuts_window final : public QDialog
	{
	public:
		explicit keyboard_shortcuts_window(QWidget* aParent = nullptr);

		/// \brief The search field, which takes the keyboard when the window opens.
		[[nodiscard]] auto search_field() const noexcept -> QLineEdit*;
		/// \brief The grouped list.
		[[nodiscard]] auto list() const noexcept -> QTreeWidget*;
		/// \brief The line shown in place of the list when nothing matches.
		[[nodiscard]] auto empty_label() const noexcept -> QLabel*;
		/// \brief The commands whose rows the current search shows, in order.
		[[nodiscard]] auto shown_commands() const -> std::vector<shortcut_command>;
		/// \brief The groups whose headings the current search shows, in order.
		[[nodiscard]] auto shown_groups() const -> std::vector<shortcut_group>;

	protected:
		void               changeEvent(QEvent* aEvent) override;
		void               showEvent(QShowEvent* aEvent) override;
		void               resizeEvent(QResizeEvent* aEvent) override;
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void populate();
		void apply_search(const QString& aText);
		void refresh_layout();
		void update_minimum_size();

		QLineEdit*   Search        = nullptr;
		QTreeWidget* List          = nullptr;
		QLabel*      Empty         = nullptr;
		QLabel*      Footer        = nullptr;
		bool         LayoutPending = false;
	};
}
#endif
