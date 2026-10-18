#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_THREAD_PICK_GESTURES_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_THREAD_PICK_GESTURES_HPP

#include "bpr/core/workbench_interaction.hpp"

#include <QString>
#include <Qt>

#include <optional>

class QAction;
class QKeyEvent;
class QMenu;
class QWidget;

/// \file
/// \brief [[BPR-1582]] and [[BPR-1356]]. How a row that stands for a thread is
/// opened, shared by Threads and Finish your review's pending comments
/// ([[BPR-1590]]): a single click selects, a double click opens in Review,
/// with Shift in Discuss; Return opens in Review, Shift+Return in Discuss;
/// the row menu offers Open in Review, Open in Discuss and, where asked,
/// Show in Threads.
namespace bpr::qt {
	/// \brief Where a double click or Return goes, by its modifiers.
	[[nodiscard]] auto thread_pick_destination(Qt::KeyboardModifiers aModifiers) noexcept -> core::workbench_thread_destination;

	/// \brief Where a key opens the row, or nothing when it is not Return or
	/// Enter.
	[[nodiscard]] auto thread_open_key(const QKeyEvent& aEvent) noexcept -> std::optional<core::workbench_thread_destination>;

	/// \brief A row menu's items, made for one opening.
	struct thread_row_menu
	{
		QMenu*   Menu        = nullptr;
		QAction* OpenReview  = nullptr;
		QAction* OpenDiscuss = nullptr;
		/// Present only when asked for.
		QAction* ShowThreads = nullptr;
	};

	/// \brief Makes the row menu, deleted when it closes; Open in Review is its
	/// default. Its items show the keys the row reads itself.
	[[nodiscard]] auto make_thread_row_menu(QWidget* aParent, const QString& aObjectName, const QString& aAccessibleName, const QString& aReviewTip,
											const QString& aDiscussTip, bool aShowThreads) -> thread_row_menu;
} // namespace bpr::qt

#endif
