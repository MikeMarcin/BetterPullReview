#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QIcon>
#include <QString>
#include <QSystemTrayIcon>

#include <optional>
#include <utility>

namespace bpr::qt {
	struct system_tray_notification_sink::implementation
	{
		QSystemTrayIcon*                          Tray = nullptr;
		application::notification_activation_sink Activations;
		/// The target of the payload most recently delivered. Retained so an
		/// activation can say which review the reviewer chose, and for no other
		/// reason: nothing reads it except `activate`.
		std::optional<application::queue_notification_target> LastTarget;
		std::size_t                                           Delivered = 0U;
	};

	system_tray_notification_sink::system_tray_notification_sink(
		QObject* const aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
		if (!QSystemTrayIcon::isSystemTrayAvailable()) {
			return;
		}
		Implementation->Tray = new QSystemTrayIcon{ this };
		Implementation->Tray->setObjectName(
			QStringLiteral("reviewQueueNotificationTray"));
		Implementation->Tray->setToolTip(QStringLiteral("BetterPullReview"));
		QObject::connect(
			Implementation->Tray,
			&QSystemTrayIcon::messageClicked,
			this,
			[this] {
				if (
					Implementation->Activations
					&& Implementation->LastTarget.has_value()) {
					Implementation->Activations(*Implementation->LastTarget);
				}
			});
		Implementation->Tray->show();
	}

	system_tray_notification_sink::~system_tray_notification_sink() = default;

	auto system_tray_notification_sink::permission()
		-> application::notification_permission
	{
		// No surface is a refusal rather than a pending question. There is
		// nobody to ask and nothing to ask for, and reporting
		// `not_determined` would make the coordinator ask on every delivery.
		if (Implementation->Tray == nullptr
			|| !QSystemTrayIcon::supportsMessages()) {
			return application::notification_permission::refused;
		}
		return application::notification_permission::granted;
	}

	auto system_tray_notification_sink::request_permission()
		-> application::notification_permission
	{
		// Never reached from `refused`, and there is no prompt to raise here:
		// the desktop notification surface either exists for this process or
		// it does not. Reported rather than asked.
		return permission();
	}

	void system_tray_notification_sink::deliver(
		const application::delivered_notification& aPayload)
	{
		if (Implementation->Tray == nullptr) {
			return;
		}
		Implementation->LastTarget = aPayload.Target;
		++Implementation->Delivered;
		// Title and body only. The payload's other field is where an
		// activation goes, not something to show.
		Implementation->Tray->showMessage(
			QString::fromStdString(aPayload.Title),
			QString::fromStdString(aPayload.Body),
			QSystemTrayIcon::Information);
	}

	void system_tray_notification_sink::set_activation_sink(
		application::notification_activation_sink aSink)
	{
		Implementation->Activations = std::move(aSink);
	}

	auto system_tray_notification_sink::tray_icon() const noexcept
		-> QSystemTrayIcon*
	{
		return Implementation->Tray;
	}

	auto system_tray_notification_sink::delivered() const noexcept -> std::size_t
	{
		return Implementation->Delivered;
	}
} // namespace bpr::qt
