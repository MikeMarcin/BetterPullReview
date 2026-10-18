#include "bpr/qt/thread_pick_gestures.hpp"

#include "bpr/qt/keyboard_shortcuts.hpp"

#include <QAction>
#include <QKeyEvent>
#include <QMenu>
#include <QObject>

namespace bpr::qt {
	auto thread_pick_destination(const Qt::KeyboardModifiers aModifiers) noexcept -> core::workbench_thread_destination
	{
		return aModifiers.testFlag(Qt::ShiftModifier) ? core::workbench_thread_destination::discuss : core::workbench_thread_destination::review;
	}

	auto thread_open_key(const QKeyEvent& aEvent) noexcept -> std::optional<core::workbench_thread_destination>
	{
		if (aEvent.key() != Qt::Key_Return && aEvent.key() != Qt::Key_Enter) {
			return std::nullopt;
		}
		return thread_pick_destination(aEvent.modifiers());
	}

	auto make_thread_row_menu(QWidget* const aParent, const QString& aObjectName, const QString& aAccessibleName, const QString& aReviewTip, const QString& aDiscussTip,
							  const bool aShowThreads) -> thread_row_menu
	{
		// [[BPR-1356]]. Made for this opening and gone when it closes, as the
		// code's menu is: the row reads Return and Shift+Return itself, so the
		// items only show them, and no window ever holds those keys.
		auto result = thread_row_menu{ .Menu = new QMenu{ aParent } };
		result.Menu->setObjectName(aObjectName);
		result.Menu->setAccessibleName(aAccessibleName);
		result.Menu->setToolTipsVisible(true);
		QObject::connect(result.Menu, &QMenu::aboutToHide, result.Menu, &QObject::deleteLater);
		const auto makeItem = [&result](const QString& aText, const QString& aName, const shortcut_command aKey, const QString& aTip) {
			auto* const action = result.Menu->addAction(aText);
			action->setObjectName(aName);
			action->setShortcuts(shortcut_keys(aKey));
			action->setShortcutContext(Qt::WidgetShortcut);
			action->setShortcutVisibleInContextMenu(true);
			action->setStatusTip(aTip);
			return action;
		};
		result.OpenReview  = makeItem(QStringLiteral("Open in Review"), aObjectName + QStringLiteral("OpenInReview"), shortcut_command::thread_open_review, aReviewTip);
		result.OpenDiscuss = makeItem(QStringLiteral("Open in Discuss"), aObjectName + QStringLiteral("OpenInDiscuss"), shortcut_command::thread_open_discuss, aDiscussTip);
		result.Menu->setDefaultAction(result.OpenReview);
		if (aShowThreads) {
			result.ShowThreads = makeItem(QStringLiteral("Show in Threads"), aObjectName + QStringLiteral("ShowInThreads"), shortcut_command::show_in_threads,
										  QStringLiteral("Its row in Threads, among the pull request's threads"));
		}
		return result;
	}
} // namespace bpr::qt
