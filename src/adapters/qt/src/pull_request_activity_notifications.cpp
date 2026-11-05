#include "bpr/qt/pull_request_activity_notifications.hpp"

#include <QApplication>
#include <QIcon>
#include <QString>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVariant>

#include <chrono>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The property both sinks on one tray write, so a click reaches only
		/// the one that posted last (`system_tray_notification_sink` reads it too).
		constexpr auto latest_submitter = "bprLatestNotificationSubmitter";

		[[nodiscard]] auto identity(const QObject& aSink) -> QVariant
		{
			return QVariant::fromValue(reinterpret_cast<quintptr>(&aSink));
		}
	} // namespace

	struct pull_request_activity_notification_sink::implementation
	{
		QSystemTrayIcon*                     Tray = nullptr;
		system_notification_display_boundary Display;
		std::function<void()>                Activation;
		std::size_t                          Posted = 0U;
	};

	namespace {
		void attach(pull_request_activity_notification_sink& aSink, QSystemTrayIcon& aTray, std::function<void()>& aActivation)
		{
			if (aTray.icon().isNull()) {
				aTray.setIcon(QApplication::windowIcon().isNull() && QApplication::style() != nullptr ? QApplication::style()->standardIcon(QStyle::SP_ComputerIcon) : QApplication::windowIcon());
			}
			QObject::connect(&aTray, &QSystemTrayIcon::messageClicked, &aSink, [&aSink, &aTray, &aActivation] {
				if (aTray.property(latest_submitter) == identity(aSink) && aActivation) {
					aActivation();
				}
			});
		}

		[[nodiscard]] auto native_display() -> system_notification_display_boundary
		{
			return [](QSystemTrayIcon& aTray, const QString& aTitle, const QString& aBody) {
				show_desktop_notification(aTray, aTitle, aBody);
			};
		}
	} // namespace

	pull_request_activity_notification_sink::pull_request_activity_notification_sink(QSystemTrayIcon& aTray, QObject* const aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
		Implementation->Tray    = &aTray;
		Implementation->Display = native_display();
		attach(*this, aTray, Implementation->Activation);
	}

	pull_request_activity_notification_sink::pull_request_activity_notification_sink(QObject* const aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
		if (!QSystemTrayIcon::isSystemTrayAvailable()) {
			return;
		}
		Implementation->Tray    = new QSystemTrayIcon{ this };
		Implementation->Display = native_display();
		attach(*this, *Implementation->Tray, Implementation->Activation);
		Implementation->Tray->show();
	}

	pull_request_activity_notification_sink::pull_request_activity_notification_sink(system_notification_display_boundary aDisplay, QObject* const aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
		Implementation->Tray    = new QSystemTrayIcon{ this };
		Implementation->Display = std::move(aDisplay);
		attach(*this, *Implementation->Tray, Implementation->Activation);
	}

	pull_request_activity_notification_sink::~pull_request_activity_notification_sink() = default;

	auto pull_request_activity_notification_sink::post(const std::string& aTitle, const std::string& aBody) -> bool
	{
		if (Implementation->Tray == nullptr || !Implementation->Display) {
			return false;
		}
		Implementation->Tray->setProperty(latest_submitter, identity(*this));
		Implementation->Display(*Implementation->Tray, QString::fromStdString(aTitle), QString::fromStdString(aBody));
		++Implementation->Posted;
		return true;
	}

	void pull_request_activity_notification_sink::set_activation(std::function<void()> aActivation)
	{
		Implementation->Activation = std::move(aActivation);
	}

	auto pull_request_activity_notification_sink::tray_icon() const noexcept -> QSystemTrayIcon*
	{
		return Implementation->Tray;
	}

	auto pull_request_activity_notification_sink::posted() const noexcept -> std::size_t
	{
		return Implementation->Posted;
	}

	pull_request_activity_timer::pull_request_activity_timer(std::function<void()> aFired, QObject* const aParent)
		: QObject{ aParent }
		, Timer{ new QTimer{ this } }
		, Fired{ std::move(aFired) }
	{
		Timer->setSingleShot(true);
		// A minute's poll need not wake the machine to the millisecond.
		Timer->setTimerType(Qt::VeryCoarseTimer);
		connect(Timer, &QTimer::timeout, this, [this] {
			if (Fired) {
				Fired();
			}
		});
	}

	pull_request_activity_timer::~pull_request_activity_timer() = default;

	void pull_request_activity_timer::schedule(const std::uint32_t aSeconds)
	{
		Timer->start(std::chrono::seconds{ aSeconds });
	}

	void pull_request_activity_timer::cancel()
	{
		Timer->stop();
	}

	auto pull_request_activity_timer::timer() const noexcept -> QTimer*
	{
		return Timer;
	}
} // namespace bpr::qt
