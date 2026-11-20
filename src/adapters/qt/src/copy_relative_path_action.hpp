#ifndef BPR_SRC_ADAPTERS_QT_SRC_COPY_RELATIVE_PATH_ACTION_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_COPY_RELATIVE_PATH_ACTION_HPP

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QObject>
#include <QString>

namespace bpr::qt {
	/// \brief [[BPR-2287]]. Adds "Copy Relative Path" to a file's menu.
	///
	/// \details The path is the pull request's, from the repository root, as
	/// the diff names it: a renamed file's new path. The files drawer, the
	/// overview's file list and the document tabs all offer it.
	inline auto add_copy_relative_path_action(QMenu& aMenu, const QString& aPath, const QString& aObjectName) -> QAction*
	{
		auto* const action = aMenu.addAction(QStringLiteral("Copy Relative Path"));
		action->setObjectName(aObjectName);
		QObject::connect(action, &QAction::triggered, &aMenu, [aPath] {
			if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
				clipboard->setText(aPath);
			}
		});
		return action;
	}
} // namespace bpr::qt

#endif
