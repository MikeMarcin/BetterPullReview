#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_SETTINGS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_SETTINGS_HPP

#include "bpr/qt/feedback_label_marks.hpp"

#include "bpr/core/feedback_label_columns.hpp"

#include <QListWidget>
#include <QString>

#include <cstdint>
#include <functional>

class QDropEvent;
class QEvent;
class QKeyEvent;
class QResizeEvent;

/// \file
/// \brief [[BPR-1088]]. The Overview chips row of Reviews & queue › Comment
/// labels (BPR-1079 selected design, Settings; RFC-BPR-0049 R20, R28).
/// \details Presentation only. The list shows the label columns the host
/// published, reports the selection a reviewer's gesture asks for, and holds
/// no preference of its own: a refused or failed edit is answered by the
/// published record, which the list then shows again.
namespace bpr::qt {
	/// \brief The twelve labels as a checklist: ticked ones first, in the
	/// order the Overview shows them, then a rule, then the rest in catalog
	/// order.
	/// \details Each row is a checkbox, the label's mark in the style and
	/// tints on screen, its ID and, when the list is wide enough, its catalog
	/// description. A ticked row carries a grip and can be dragged into
	/// place; Alt+Up and Alt+Down (⌥↑ ⌥↓ on macOS) move the current ticked
	/// row by one. Ticking adds a label at the end; unticking removes it.
	/// Every change reports the whole new selection once. Tooltips sit right
	/// below what they name, as every label mark's do.
	class feedback_label_columns_editor final : public QListWidget
	{
	public:
		using dispatch = std::function<void(const core::feedback_label_columns&)>;

		/// \brief The data role that holds a row's label, as its catalog index.
		static constexpr int label_role = Qt::UserRole + 1;
		/// \brief The data role that marks the rule between ticked and unticked rows.
		static constexpr int separator_role = Qt::UserRole + 2;
		/// \brief Below this width the descriptions are left out (BPR-1079's
		/// narrow frame shows the IDs only).
		static constexpr int description_width = 340;

		explicit feedback_label_columns_editor(QWidget* aParent = nullptr);

		/// \brief Shows the published selection.
		void set_columns(const core::feedback_label_columns& aColumns);
		/// \brief The selection the rows show now: ticked rows, top to bottom.
		[[nodiscard]] auto columns() const -> core::feedback_label_columns;
		void               set_theme(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto theme() const noexcept -> const feedback_mark_theme&;
		void               set_dispatch(dispatch aDispatch);
		/// \brief One label's row.
		[[nodiscard]] auto item_of(core::feedback_label aLabel) const -> QListWidgetItem*;
		/// \brief The rule under the ticked rows; hidden while none or all are ticked.
		[[nodiscard]] auto separator_item() const noexcept -> QListWidgetItem*;
		/// \brief Moves the current ticked row one place up (`-1`) or down
		/// (`+1`) among the ticked rows and reports the new order.
		auto move_current(int aStep) -> bool;
		/// \brief Whether rows show their descriptions at the current width.
		[[nodiscard]] auto descriptions_shown() const noexcept -> bool;
		/// \brief The words of a row's tooltip: its description and, on a
		/// ticked row, how to reorder it.
		[[nodiscard]] auto row_tooltip(const QListWidgetItem& aItem) const -> QString;
		/// \brief "Drag to reorder (⌥↑ ⌥↓)", with this platform's key names.
		[[nodiscard]] static auto reorder_hint() -> QString;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		void dropEvent(QDropEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// \brief Puts the rows in the order a selection shows, keeping the
		/// current row.
		void arrange(const core::feedback_label_columns& aColumns);
		/// \brief Refreshes each row's check, mark, names and accessible text.
		void refresh_rows();
		/// \brief Reports the shown selection when it differs from the last.
		void report();
		void fit_height();

		std::array<QListWidgetItem*, core::feedback_label_count> Rows{ };
		QListWidgetItem*                                         Separator = nullptr;
		core::feedback_label_columns                             Shown;
		feedback_mark_theme                                      Theme;
		dispatch                                                 Dispatch;
		bool                                                     Applying = false;
	};

	/// \brief What the Overview chips row says when the stored list named
	/// labels it had to leave out: how many, and how to save a clean list.
	[[nodiscard]] auto feedback_label_columns_recovery_text(std::uint8_t aUnknown, std::uint8_t aRepeated) -> QString;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FEEDBACK_LABEL_SETTINGS_HPP
