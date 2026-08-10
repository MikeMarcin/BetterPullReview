#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QIcon>
#include <QString>
#include <QSystemTrayIcon>

#include <optional>
#include <utility>

namespace bpr::qt {
	struct system_tray_notification_sink::implementation
	{
		QSystemTrayIcon*                                      Tray = nullptr;
		application::notification_activation_sink             Activations;
		std::optional<application::queue_notification_target> FirstTarget;
		std::size_t                                           Submitted          = 0U;
		bool                                                  ActivationConsumed = false;
	};

	namespace {
		void connect_activation(
			system_tray_notification_sink& aSink,
			QSystemTrayIcon&               aTray,
			auto&                          aImplementation)
		{
			QObject::connect(
				&aTray,
				&QSystemTrayIcon::messageClicked,
				&aSink,
				[&aImplementation] {
					if (!aImplementation.FirstTarget.has_value()
						|| aImplementation.ActivationConsumed) {
						return;
					}
					aImplementation.ActivationConsumed = true;
					if (aImplementation.Activations) {
						aImplementation.Activations(
							*aImplementation.FirstTarget);
					}
				});
		}
	} // namespace

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
		connect_activation(*this, *Implementation->Tray, *Implementation);
		Implementation->Tray->show();
	}

	system_tray_notification_sink::system_tray_notification_sink(
		QSystemTrayIcon& aTray,
		QObject* const   aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
		Implementation->Tray = &aTray;
		connect_activation(*this, aTray, *Implementation);
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

	auto system_tray_notification_sink::deliver(
		const application::delivered_notification& aPayload)
		-> application::notification_submission_result
	{
		if (Implementation->FirstTarget.has_value()) {
			return application::notification_submission_result::
				suppressed_unattributable_activation;
		}
		Implementation->FirstTarget = aPayload.Target;
		++Implementation->Submitted;
		// Title and body only. The payload's other field is where an
		// activation goes, not something to show.
		if (Implementation->Tray != nullptr) {
			Implementation->Tray->showMessage(
				QString::fromStdString(aPayload.Title),
				QString::fromStdString(aPayload.Body),
				QSystemTrayIcon::Information);
		}
		return application::notification_submission_result::submitted;
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

	auto system_tray_notification_sink::submitted() const noexcept -> std::size_t
	{
		return Implementation->Submitted;
	}
} // namespace bpr::qt
