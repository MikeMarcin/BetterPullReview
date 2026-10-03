#include "bpr/qt/menu_bar_status_item.hpp"

#include <QPixmap>
#include <QString>
#include <QSystemTrayIcon>

namespace bpr::qt {
	auto menu_bar_status_icon() -> QIcon
	{
		// Both images explicitly: Qt looks for an @2x partner only when the
		// application's own ratio is above 1, and a Mac can have a 1x screen
		// beside a 2x one.
		auto single  = QPixmap{ QStringLiteral(":/bpr/status-item/dif-template.png") };
		auto doubled = QPixmap{ QStringLiteral(":/bpr/status-item/dif-template@2x.png") };
		if (single.isNull() || doubled.isNull()) {
			return { };
		}
		single.setDevicePixelRatio(1.0);
		doubled.setDevicePixelRatio(2.0);
		auto icon = QIcon{ };
		icon.addPixmap(single);
		icon.addPixmap(doubled);
		icon.setIsMask(true);
		return icon;
	}

	auto make_menu_bar_status_item() -> std::shared_ptr<QSystemTrayIcon>
	{
		if (!QSystemTrayIcon::isSystemTrayAvailable()) {
			return { };
		}
		auto item = std::make_shared<QSystemTrayIcon>(menu_bar_status_icon());
		item->setObjectName(QStringLiteral("menuBarStatusItem"));
		item->setToolTip(QStringLiteral("BetterPullReview"));
		item->show();
		return item;
	}
} // namespace bpr::qt
