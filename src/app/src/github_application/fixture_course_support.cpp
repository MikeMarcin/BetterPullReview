#include "github_application/fixture_course_support.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/qt/workbench_drawer.hpp"
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QByteArray>
	#include <QDesktopServices>
	#include <QListView>
	#include <QVariant>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto drawer_row(
		const qt::workbench_window& aWindow,
		const std::string_view      aPath) -> QModelIndex
	{
		if (aWindow.files_drawer() == nullptr
			|| aWindow.files_drawer()->contents_view() == nullptr
			|| aWindow.files_drawer()->contents_view()->model() == nullptr) {
			return { };
		}
		const auto* const model    = aWindow.files_drawer()->contents_view()->model();
		const auto        expected = QString::fromUtf8(
			aPath.data(), static_cast<qsizetype>(aPath.size()));
		for (auto row = 0; row < model->rowCount(); ++row) {
			const auto index = model->index(row, 0);
			if (index.data(Qt::DisplayRole).toString() == expected) {
				return index;
			}
		}
		return { };
	}
#endif
}
