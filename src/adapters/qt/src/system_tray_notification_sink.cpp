#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QVariant>

#include <optional>
#include <utility>

namespace bpr::qt {
	struct system_tray_notification_sink::implementation
	{
		QSystemTrayIcon*                                   Tray = nullptr;
		system_notification_display_boundary               Display;
		application::notification_activation_sink          Activations;
		std::optional<application::delivered_notification> FirstPayload;
		std::size_t                                        Submitted           = 0U;
		bool                                               SubmissionRequested = false;
		bool                                               ActivationConsumed  = false;
	};

	namespace {
		[[nodiscard]] auto notification_icon() -> QIcon
		{
			auto icon = QApplication::windowIcon();
			if (icon.isNull() && QApplication::style() != nullptr) {
				icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
			}
			if (icon.isNull()) {
				auto fallback = QPixmap{ 16, 16 };
				fallback.fill(QColor{ 52, 101, 164 });
				icon.addPixmap(fallback);
			}
			return icon;
		}

		void prepare_tray(QSystemTrayIcon& aTray)
		{
			if (aTray.icon().isNull()) {
				aTray.setIcon(notification_icon());
			}
			// A shared status item keeps the name and tip its owner gave it.
			if (aTray.objectName().isEmpty()) {
				aTray.setObjectName(QStringLiteral("reviewQueueNotificationTray"));
			}
			if (aTray.toolTip().isEmpty()) {
				aTray.setToolTip(QStringLiteral("BetterPullReview"));
			}
		}

		[[nodiscard]] auto native_display_boundary()
			-> system_notification_display_boundary
		{
			return [](QSystemTrayIcon& aTray, const QString& aTitle, const QString& aBody) {
				aTray.showMessage(aTitle, aBody, QSystemTrayIcon::Information);
			};
		}

		/// The tray property naming the sink that submitted to it last.
		///
		/// \details [[BPR-1250]]. On macOS every profile's sink submits through the
		/// application's one status item. `messageClicked` carries no message
		/// identity, so a click goes to the latest submission and to no other sink;
		/// otherwise one click would activate every profile that ever submitted.
		/// The value is compared, never dereferenced.
		constexpr auto latest_submitter = "bprLatestNotificationSubmitter";

		[[nodiscard]] auto submitter_identity(const system_tray_notification_sink& aSink)
			-> QVariant
		{
			return QVariant::fromValue(reinterpret_cast<quintptr>(&aSink));
		}

		void connect_activation(
			system_tray_notification_sink& aSink,
			QSystemTrayIcon&               aTray,
			auto&                          aImplementation)
		{
			QObject::connect(
				&aTray,
				&QSystemTrayIcon::messageClicked,
				&aSink,
				[&aSink, &aTray, &aImplementation] {
					if (!aImplementation.FirstPayload.has_value()
						|| aImplementation.ActivationConsumed
						|| aTray.property(latest_submitter) != submitter_identity(aSink)) {
						return;
					}
					aImplementation.ActivationConsumed = true;
					if (aImplementation.Activations) {
						aImplementation.Activations(
							aImplementation.FirstPayload->Target);
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
		Implementation->Tray    = new QSystemTrayIcon{ this };
		Implementation->Display = native_display_boundary();
		prepare_tray(*Implementation->Tray);
		connect_activation(*this, *Implementation->Tray, *Implementation);
		Implementation->Tray->show();
	}

	system_tray_notification_sink::system_tray_notification_sink(
		QSystemTrayIcon& aTray,
		QObject* const   aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
		Implementation->Tray    = &aTray;
		Implementation->Display = native_display_boundary();
		prepare_tray(aTray);
		connect_activation(*this, aTray, *Implementation);
	}

	system_tray_notification_sink::system_tray_notification_sink(
		system_notification_display_boundary aDisplay,
		QObject* const                       aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
		Implementation->Tray    = new QSystemTrayIcon{ this };
		Implementation->Display = std::move(aDisplay);
		prepare_tray(*Implementation->Tray);
		connect_activation(*this, *Implementation->Tray, *Implementation);
	}

	system_tray_notification_sink::~system_tray_notification_sink() = default;

	auto system_tray_notification_sink::permission()
		-> application::notification_permission
	{
		// No surface is a refusal rather than a pending question. There is
		// nobody to ask and nothing to ask for, and reporting
		// `not_determined` would make the coordinator ask on every delivery.
		if (Implementation->Tray == nullptr || !Implementation->Display) {
			return application::notification_permission::refused;
		}
		return Implementation->SubmissionRequested
			? application::notification_permission::platform_managed
			: application::notification_permission::not_determined;
	}

	auto system_tray_notification_sink::request_permission()
		-> application::notification_permission
	{
		if (Implementation->Tray == nullptr || !Implementation->Display) {
			return application::notification_permission::refused;
		}
		// Qt exposes no owner-permission query. Authorize one submission attempt;
		// the platform owns any prompt and whether a banner is actually shown.
		Implementation->SubmissionRequested = true;
		return application::notification_permission::platform_managed;
	}

	auto system_tray_notification_sink::deliver(
		const application::delivered_notification& aPayload)
		-> application::notification_submission_result
	{
		if (Implementation->FirstPayload.has_value()) {
			return application::notification_submission_result::
				suppressed_unattributable_activation;
		}
		Implementation->FirstPayload = aPayload;
		++Implementation->Submitted;
		// Title and body only. The payload's other field is where an
		// activation goes, not something to show.
		if (Implementation->Tray != nullptr && Implementation->Display) {
			Implementation->Tray->setProperty(latest_submitter, submitter_identity(*this));
			Implementation->Display(
				*Implementation->Tray,
				QString::fromStdString(aPayload.Title),
				QString::fromStdString(aPayload.Body));
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

	auto system_tray_notification_sink::submitted_payload() const
		-> std::optional<application::delivered_notification>
	{
		return Implementation->FirstPayload;
	}
} // namespace bpr::qt
